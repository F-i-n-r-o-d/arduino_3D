#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_NeoPixel.h>

// Predpokladany hardware: ESP32 + MPU6050 + WS2812B (60 LED).
// Pro jine zapojeni zmen LED_PIN, LED_COUNT, I2C piny.
#define LED_PIN 5
#define LED_COUNT 60
#define I2C_SDA 21
#define I2C_SCL 22

Adafruit_MPU6050 mpu;
Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

const int CENTER = LED_COUNT / 2;
const float MAX_ANGLE = 24.0f;
const unsigned long OUTSIDE_LIMIT_MS = 1500;
const unsigned long FRAME_MS = 25;
const int BONUS_TIMES[] = {15, 30, 60, 120};
const int BONUS_POINTS[] = {100, 250, 600, 1500};

float zeroAngle = 0.0f;
float filteredAngle = 0.0f;
unsigned long lastFrame = 0;
unsigned long lastScorePrint = 0;
unsigned long outsideSince = 0;
unsigned long safeMillis = 0;
int lives = 3;
int bonus = 0;
int milestone = 0;
int record = 0;
bool finished = false;
unsigned long restartAt = 0;

float readAngle() {
  sensors_event_t acceleration, gyro, temperature;
  mpu.getEvent(&acceleration, &gyro, &temperature);
  return atan2f(acceleration.acceleration.y, acceleration.acceleration.z) * 180.0f / PI;
}

int score() { return (int)(safeMillis / 100UL) + bonus; }

int halfZone() {
  int z = 6 - (int)(safeMillis / 20000UL);
  return max(2, z);
}

void render(int dot, bool safe) {
  strip.clear();
  for (int i = CENTER - halfZone(); i <= CENTER + halfZone(); ++i) {
    if (i >= 0 && i < LED_COUNT) strip.setPixelColor(i, strip.Color(0, 45, 0));
  }
  strip.setPixelColor(dot, safe ? strip.Color(0, 0, 130) : strip.Color(130, 0, 0));
  strip.show();
}

void flash(uint32_t color) {
  for (int i = 0; i < LED_COUNT; ++i) strip.setPixelColor(i, color);
  strip.show();
  delay(250);
}

void startGame() {
  strip.clear();
  strip.show();
  delay(300);
  zeroAngle = readAngle();
  filteredAngle = zeroAngle;
  safeMillis = 0;
  outsideSince = 0;
  lives = 3;
  bonus = 0;
  milestone = 0;
  finished = false;
  lastFrame = millis();
  Serial.println("NOVA HRA - drz LED bod ve stredu!");
}

void setup() {
  Serial.begin(115200);
  Wire.begin(I2C_SDA, I2C_SCL);
  strip.begin();
  strip.setBrightness(90);
  strip.show();

  if (!mpu.begin()) {
    Serial.println("CHYBA: MPU6050 nenalezen. Zkontroluj zapojeni.");
    while (true) {
      flash(strip.Color(100, 0, 0));
      strip.clear(); strip.show();
      delay(350);
    }
  }
  mpu.setAccelerometerRange(MPU6050_RANGE_4_G);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
  delay(700);
  startGame();
}

void loop() {
  unsigned long now = millis();
  if (finished) {
    if ((long)(now - restartAt) >= 0) startGame();
    return;
  }
  unsigned long dt = now - lastFrame;
  if (dt < FRAME_MS) return;
  lastFrame = now;
  if (dt > 150) dt = FRAME_MS;

  float angle = readAngle();
  filteredAngle = 0.82f * filteredAngle + 0.18f * angle;
  float relative = filteredAngle - zeroAngle;
  int dot = CENTER + (int)roundf(relative * (LED_COUNT / 2.0f - 1) / MAX_ANGLE);
  dot = constrain(dot, 0, LED_COUNT - 1);
  bool safe = abs(dot - CENTER) <= halfZone();

  if (safe) {
    safeMillis += dt;
    outsideSince = 0;
    while (milestone < 4 && safeMillis >= (unsigned long)BONUS_TIMES[milestone] * 1000UL) {
      bonus += BONUS_POINTS[milestone];
      Serial.print("BONUS +"); Serial.println(BONUS_POINTS[milestone]);
      milestone++;
    }
  } else {
    if (outsideSince == 0) outsideSince = now;
    if (now - outsideSince >= OUTSIDE_LIMIT_MS) {
      lives--;
      outsideSince = 0;
      Serial.print("CHYBA! Zivoty: "); Serial.println(lives);
      flash(strip.Color(130, 0, 0));
      if (lives <= 0) {
        finished = true;
        record = max(record, score());
        Serial.print("KONEC HRY | Body: "); Serial.print(score());
        Serial.print(" | Rekord: "); Serial.println(record);
        flash(strip.Color(0, 0, 110));
        restartAt = millis() + 3000UL;
        return;
      }
      // Nova pozice stredu se nastavi podle aktualni polohy tyce.
      zeroAngle = filteredAngle;
    }
  }

  render(dot, safe);
  if (now - lastScorePrint >= 1000) {
    lastScorePrint = now;
    Serial.print("Cas: "); Serial.print(safeMillis / 1000UL);
    Serial.print(" s | Body: "); Serial.print(score());
    Serial.print(" | Zivoty: "); Serial.println(lives);
  }
}
