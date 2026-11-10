
#include <NeoPixelConnect.h>
#include <Arduino_LSM6DSOX.h>
#include <math.h>

#define LED_PIN 4
#define LED_COUNT 64

#define DEAD_ZONE 3.0
#define MAX_ANGLE 45.0

#define MIN_SPEED 0.5
#define MAX_SPEED 35.0

#define BASE_BLUE 15
#define MAX_BLUE 130
#define MAX_GREEN 85

NeoPixelConnect np(LED_PIN, LED_COUNT, pio0, 0);

float x = 0;
float y = 0;
float z = 0;

float zeroAngle = 0;
float tilt = 0;
float filteredTilt = 0;

float flowPosition = 0;
float flowSpeed = 0.5;

int direction = 1;

unsigned long lastSensor = 0;
unsigned long lastFrame = 0;

bool sensorOK = false;

// ===================================
// VYPOCET NAKLONU
// ===================================

float calculateTilt(float x, float y, float z) {
  return atan2(x, sqrt(y * y + z * z))
         * 180.0 / PI;
}

// ===================================
// VYKRESLENI VODY
// ===================================

void drawWater() {

  for (int i = 0; i < LED_COUNT; i++) {

    float position = i - flowPosition;

    float wave1 = sin(position * 0.35);
    float wave2 = sin(position * 0.75 + 1.5);
    float wave3 = sin(position * 0.15 - 2.0);

    float brightness =
        0.45
        + 0.25 * wave1
        + 0.18 * wave2
        + 0.12 * wave3;

    brightness = constrain(brightness, 0.05, 1.0);

    int blue = BASE_BLUE + brightness * MAX_BLUE;
    int green = 5 + brightness * MAX_GREEN;

    np.neoPixelSetValue(
      (uint8_t)i,
      0,
      green,
      blue,
      false
    );
  }

  np.neoPixelShow();
}

// ===================================
// KALIBRACE
// ===================================

void calibrate() {

  Serial.println("Drz tyc rovne - kalibrace");

  float total = 0;
  int count = 0;

  unsigned long start = millis();

  while (millis() - start < 2000) {

    if (IMU.accelerationAvailable()) {

      IMU.readAcceleration(x, y, z);

      total += calculateTilt(x, y, z);
      count++;
    }

    delay(5);
  }

  if (count > 0) {
    zeroAngle = total / count;
  }

  filteredTilt = 0;

  Serial.println("Kalibrace hotova");
}

// ===================================
// SETUP
// ===================================

void setup() {

  Serial.begin(115200);
  delay(1500);

  Serial.println("START LED VODOPAD");

  // Nejdřív otestujeme LED
  np.neoPixelFill(0, 0, 100, true);

  delay(1000);

  np.neoPixelClear(true);

  // Spustime senzor
  if (!IMU.begin()) {

    Serial.println("CHYBA: IMU nenalezena!");

    // LED cervene = chyba senzoru
    np.neoPixelFill(100, 0, 0, true);

    while (true) {
      delay(100);
    }
  }

  Serial.println("IMU OK");

  calibrate();

  lastSensor = millis();
  lastFrame = millis();

  // Prvni vykresleni vody
  drawWater();

  Serial.println("VODOPAD SPUSTEN");
}

// ===================================
// LOOP
// ===================================

void loop() {

  unsigned long now = millis();

  // CTENI AKCELEROMETRU
  if (now - lastSensor >= 10) {

    lastSensor = now;

    if (IMU.accelerationAvailable()) {

      IMU.readAcceleration(x, y, z);

      tilt = calculateTilt(x, y, z) - zeroAngle;

      filteredTilt =
        filteredTilt * 0.85
        + tilt * 0.15;
    }
  }

  // SMER VODY
  if (filteredTilt > DEAD_ZONE) {
    direction = 1;
  }
  else if (filteredTilt < -DEAD_ZONE) {
    direction = -1;
  }

  // RYCHLOST PODLE NAKLONU
  float angle = abs(filteredTilt);

  angle = constrain(angle, 0, MAX_ANGLE);

  flowSpeed =
    MIN_SPEED +
    (angle / MAX_ANGLE) *
    (MAX_SPEED - MIN_SPEED);

  // POHYB VODY
  float dt = (now - lastFrame) / 1000.0;

  if (dt > 0.1) dt = 0.1;

  flowPosition += direction * flowSpeed * dt;

  while (flowPosition >= LED_COUNT) {
    flowPosition -= LED_COUNT;
  }

  while (flowPosition < 0) {
    flowPosition += LED_COUNT;
  }

  // VYKRESLOVANI
  if (now - lastFrame >= 20) {

    lastFrame = now;

    drawWater();
  }
}
