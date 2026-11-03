
#include <NeoPixelConnect.h>
#include <Arduino_LSM6DSOX.h>
#include <math.h>

#define MAXIMUM_NUM_NEOPIXELS 64
#define LED_PIN 4

// Nastaveni vody
#define DEAD_ZONE 3.0
#define MIN_SPEED 2.0
#define MAX_SPEED 38.0

// Jas vody
#define MAX_BLUE 100
#define MAX_GREEN 65

NeoPixelConnect np(
  LED_PIN,
  MAXIMUM_NUM_NEOPIXELS,
  pio0,
  0
);

float x, y, z;

// Naklon
float tilt = 0;
float filteredTilt = 0;
float zeroTilt = 0;

// Smer proudu
int direction = 1;

// Pozice tekouci vody
float flowPosition = 0;

// Casovani
unsigned long lastUpdate = 0;
unsigned long lastSensorRead = 0;

// Rychlost vody
float flowSpeed = MIN_SPEED;


// =================================
// VYPOCET NAKLONU
// =================================

float getTilt(float x, float y, float z) {
  return atan2(
    x,
    sqrt(y * y + z * z)
  ) * 180.0 / PI;
}


// =================================
// KALIBRACE SENZORU
// =================================

void calibrateSensor() {

  Serial.println("Kalibrace...");
  Serial.println("Drz tyc rovne!");

  float sum = 0;
  int samples = 0;

  for (int i = 0; i < 100; i++) {

    if (IMU.accelerationAvailable()) {

      IMU.readAcceleration(x, y, z);

      sum += getTilt(x, y, z);
      samples++;
    }

    delay(10);
  }

  if (samples > 0) {
    zeroTilt = sum / samples;
  }

  filteredTilt = 0;

  Serial.print("Nulovy naklon: ");
  Serial.println(zeroTilt);

  Serial.println("Kalibrace hotova!");
}


// =================================
// VYKRESLENI VODY
// =================================

void drawWater() {

  for (int i = 0; i < MAXIMUM_NUM_NEOPIXELS; i++) {

    // Plynula vlna vody
    float wave = sin(
      (i - flowPosition) * 0.45
    );

    wave = (wave + 1.0) / 2.0;

    // Zvyrazneni svetelnych vln
    float brightness = wave * wave * wave;

    // Jemne dalsi odlesky
    float detail = sin(
      (i - flowPosition) * 1.1
    );

    detail = (detail + 1.0) / 2.0;

    int blue = 5 + brightness * MAX_BLUE;

    int green = 2
      + brightness * MAX_GREEN
      + detail * 10;

    int red = 0;

    blue = constrain(blue, 0, 255);
    green = constrain(green, 0, 255);

    np.neoPixelSetValue(
      (uint8_t)i,
      red,
      green,
      blue,
      false
    );
  }

  np.neoPixelShow();
}


// =================================
// SETUP
// =================================

void setup() {

  Serial.begin(115200);

  delay(1500);

  Serial.println("VODNI LED TY C");

  // Spusteni senzoru
  if (!IMU.begin()) {

    Serial.println("CHYBA IMU!");

    while (1) {
      delay(100);
    }
  }

  Serial.println("IMU OK!");

  // Kalibrace
  calibrateSensor();

  lastUpdate = millis();
  lastSensorRead = millis();

  Serial.println("Vodopad spusten!");
}


// =================================
// HLAVNI PROGRAM
// =================================

void loop() {

  unsigned long now = millis();

  // =================================
  // CTENI SENZORU
  // =================================

  if (now - lastSensorRead >= 10) {

    lastSensorRead = now;

    if (IMU.accelerationAvailable()) {

      IMU.readAcceleration(x, y, z);

      tilt = getTilt(x, y, z) - zeroTilt;

      // Vyhlazeni pohybu
      filteredTilt =
        filteredTilt * 0.85
        + tilt * 0.15;
    }
  }


  // =================================
  // SMER VODY
  // =================================

  if (filteredTilt > DEAD_ZONE) {

    direction = 1;

  }

  if (filteredTilt < -DEAD_ZONE) {

    direction = -1;

  }


  // =================================
  // RYCHLOST PODLE NAKLONU
  // =================================

  float angle = abs(filteredTilt);

  angle = constrain(angle, 0, 60);

  flowSpeed = MIN_SPEED
    + (angle / 60.0)
    * (MAX_SPEED - MIN_SPEED);


  // =================================
  // POHYB VODY
  // =================================

  float deltaTime =
    (now - lastUpdate) / 1000.0;

  lastUpdate = now;

  flowPosition +=
    direction * flowSpeed * deltaTime;

  // Smycka LED pasku
  while (flowPosition >= MAXIMUM_NUM_NEOPIXELS) {
    flowPosition -= MAXIMUM_NUM_NEOPIXELS;
  }

  while (flowPosition < 0) {
    flowPosition += MAXIMUM_NUM_NEOPIXELS;
  }


  // =================================
  // VYKRESLENI
  // =================================

  drawWater();


  // =================================
  // LADENI
  // =================================

  Serial.print("Naklon: ");
  Serial.print(filteredTilt);

  Serial.print(" | Smer: ");

  if (direction == 1) {
    Serial.print("DOPRAVA");
  } else {
    Serial.print("DOLEVA");
  }

  Serial.print(" | Rychlost: ");
  Serial.println(flowSpeed);

  delay(5);
}
