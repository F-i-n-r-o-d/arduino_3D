
#if __has_include(<Wire.h>)
  #include <Wire.h>
#endif

#if __has_include(<Adafruit_NeoPixel.h>)
  #include <Adafruit_NeoPixel.h>
#elif !defined(ARDUINO)
  #define NEO_GRB 0x01
  #define NEO_KHZ800 0x02
  class Adafruit_NeoPixel {
  public:
    Adafruit_NeoPixel(uint16_t count, uint8_t pin, uint8_t flags) {}
    void begin() {}
    void setBrightness(uint8_t) {}
    void clear() {}
    void show() {}
    void setPixelColor(uint16_t, uint32_t) {}
    uint32_t Color(uint8_t r, uint8_t g, uint8_t b) {
      return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
    }
  };
#endif

#if __has_include(<Adafruit_MPU6050.h>)
  #include <Adafruit_MPU6050.h>
#elif !defined(ARDUINO)
  struct Vector3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
  };

  struct sensors_event_t {
    Vector3 acceleration;
    Vector3 gyro;
    float temperature = 0.0f;
  };

  enum {
    MPU6050_RANGE_4_G = 0,
    MPU6050_BAND_21_HZ = 0
  };

  class Adafruit_MPU6050 {
  public:
    bool begin() { return true; }
    void setAccelerometerRange(int) {}
    void setFilterBandwidth(int) {}
    void getEvent(sensors_event_t* a, sensors_event_t* g, sensors_event_t* temp) {
      if (a) { *a = sensors_event_t(); }
      if (g) { *g = sensors_event_t(); }
      if (temp) { *temp = sensors_event_t(); }
    }
  };
#endif

#if __has_include(<Adafruit_Sensor.h>)
  #include <Adafruit_Sensor.h>
#endif

#include <math.h>

#ifndef ARDUINO
  class Serial_ {
  public:
    void begin(unsigned long) {}
    void println(const char*) {}
    void println(float) {}
    void print(const char*) {}
    void print(float) {}
  };

  extern Serial_ Serial;

  inline unsigned long millis() { return 0; }
  inline void delay(unsigned long) {}
#endif

// ===== NASTAVENI =====

#define LED_PIN 6
#define LED_COUNT 60

// Citlivost naklonu ve stupnich
#define DEAD_ZONE 4.0

// Rychlost pri malem a velkem naklonu
#define SLOW_SPEED 140
#define FAST_SPEED 12

// Jas LED 0-255
#define LED_BRIGHTNESS 100

// ====================

Adafruit_NeoPixel leds(
  LED_COUNT,
  LED_PIN,
  NEO_GRB + NEO_KHZ800
);

Adafruit_MPU6050 mpu;

float tilt = 0;
float filteredTilt = 0;

int direction = 1;
float speedLevel = 0;

unsigned long lastUpdate = 0;
unsigned long lastSensorRead = 0;

int flowPosition = 0;

void setup() {
  Serial.begin(115200);
  Wire.begin();

  leds.begin();
  leds.setBrightness(LED_BRIGHTNESS);
  leds.clear();
  leds.show();

  if (!mpu.begin()) {
    Serial.println("CHYBA: MPU6050 nenalezen!");
    while (true) {
      delay(100);
    }
  }

  mpu.setAccelerometerRange(
    MPU6050_RANGE_4_G
  );

  mpu.setFilterBandwidth(
    MPU6050_BAND_21_HZ
  );

  Serial.println("LED vodni tyc spustena!");
}

void loop() {
  // ===== CTENI SENZORU =====

  if (millis() - lastSensorRead >= 10) {
    lastSensorRead = millis();

    sensors_event_t a, g, temp;
    mpu.getEvent(&a, &g, &temp);

    // Vypocet naklonu
    tilt = atan2(
      a.acceleration.x,
      a.acceleration.z
    ) * 180.0 / PI;

    // Vyhlazeni hodnoty
    filteredTilt =
      filteredTilt * 0.80 + tilt * 0.20;

    Serial.print("Naklon: ");
    Serial.println(filteredTilt);
  }

  // ===== SMER PROUDENI =====

  if (filteredTilt > DEAD_ZONE) {
    direction = 1;
  }
  else if (filteredTilt < -DEAD_ZONE) {
    direction = -1;
  }

  // ===== RYCHLOST PODLE NAKLONU =====

  speedLevel = abs(filteredTilt);

  speedLevel = constrain(
    speedLevel,
    0,
    60
  );

  int interval = map(
    (int)speedLevel,
    0,
    60,
    SLOW_SPEED,
    FAST_SPEED
  );

  // Pri malem naklonu zustane animace pomala
  if (abs(filteredTilt) <= DEAD_ZONE) {
    interval = SLOW_SPEED;
  }

  // ===== POSUN VODNIHO PROUDU =====

  if (millis() - lastUpdate >=
      (unsigned long)interval) {

    lastUpdate = millis();

    flowPosition += direction;

    // Konec pasku -> zacatek
    if (flowPosition >= LED_COUNT) {
      flowPosition = 0;
    }

    if (flowPosition < 0) {
      flowPosition = LED_COUNT - 1;
    }
  }

  // ===== VYKRESLENI VODY =====

  for (int i = 0; i < LED_COUNT; i++) {

    // Vzdalenost od stredu proudu
    int distance = abs(i - flowPosition);

    // Plynuly prechod pres konec pasku
    distance = min(
      distance,
      LED_COUNT - distance
    );

    // Jas hlavni vlny
    int brightness = 255 - distance * 35;

    brightness = constrain(
      brightness,
      0,
      255
    );

    // Trochu promenlive barvy vody
    int blue = brightness;
    int green = brightness * 0.55;

    // Jemne svetlejsi stred proudu
    if (distance == 0) {
      green = min(255, green + 50);
    }

    leds.setPixelColor(
      i,
      leds.Color(0, green, blue)
    );
  }

  leds.show();

  delay(2);
}
