#include <NeoPixelConnect.h>
#include <Arduino_LSM6DSOX.h>

#define POCET_LED 16

NeoPixelConnect np(4, POCET_LED, pio0, 0);

float x, y, z;
float xf = 0;

void cti() {
  if (IMU.accelerationAvailable()) {
    IMU.readAcceleration(x, y, z);
    xf = 0.8 * xf + 0.2 * x;
  }
}

int mapFloat(float v, float in_min, float in_max, int out_min, int out_max) {
  return (v - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

void svit(int p, int r, int g, int b) {
  np.neoPixelClear(false);
  np.neoPixelSetValue(p, r, g, b, false);
  np.neoPixelShow();
}

void setup() {
  Serial.begin(115200);
  if (!IMU.begin()) {
    Serial.println("Chyba IMU!");
    while (1);
  }
  randomSeed(analogRead(A0));
}

void loop() {
  int cil = random(0, POCET_LED);
  svit(cil, 120, 60, 0);
  delay(2000);

  int kulicka = POCET_LED / 2;
  unsigned long drziOd = 0;

  while (true) {
    cti();
    bool zamyka = (y > 0.5);

    if (!zamyka) {
      kulicka = mapFloat(xf, -0.5, 0.5, 0, POCET_LED - 1);
      if (kulicka > POCET_LED - 1) kulicka = POCET_LED - 1;
      if (kulicka < 0) kulicka = 0;
      drziOd = 0;
    } else {
      if (drziOd == 0) drziOd = millis();
      if (millis() - drziOd >= 1000) break;
    }

    if (zamyka) svit(kulicka, 80, 0, 120);
    else        svit(kulicka, 0, 20, 120);
    delay(10);
  }

  int vzdalenost = abs(kulicka - cil);
  int presnost = 100 - vzdalenost * 100 / (POCET_LED - 1);

  Serial.print("Cil: ");      Serial.println(cil + 1);
  Serial.print("Tip: ");      Serial.println(kulicka + 1);
  Serial.print("Presnost: "); Serial.print(presnost); Serial.println(" %");

  np.neoPixelClear(false);
  np.neoPixelSetValue(cil, 0, 120, 0, false);
  if (kulicka != cil) np.neoPixelSetValue(kulicka, 120, 0, 0, false);
  np.neoPixelShow();
  delay(4000);
}