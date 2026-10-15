// LED hra - Arduino Nano RP2040 Connect + LED pásek (64 diod na D4) + vestavěný LSM6DSOX
// Základ: původní kód (náklon -> červená dioda, zelená cílová zóna), rozšířený o hru.
//
// Komunikace s webovou stránkou přes USB (Serial 115200):
//   Arduino -> stránka:  HIT 1 / HIT 2          (trefa hráče 1 / 2)
//   stránka -> Arduino:  START, END, LEVEL n, MODE solo|versus|coop

#include <NeoPixelConnect.h>   // knihovna pro led pásek
#include <Arduino_LSM6DSOX.h>  // knihovna pro čtení náklonu

#define NUM_LEDS 64            // délka pásku

NeoPixelConnect np(4, NUM_LEDS, pio0, 0);   // pásek na digitálním pinu D4

// barvy cílové zóny: hráč 1 = zelená, hráč 2 = fialová
const uint8_t ZR[2] = {0, 40};
const uint8_t ZG[2] = {40, 0};
const uint8_t ZB[2] = {0, 40};

bool running = false;      // běží hra?
int level = 1;             // 1..10 (posílá stránka)
int mode = 0;              // 0 = sólo, 1 = proti sobě, 2 = spolu
int player = 0;            // kdo je zrovna na řadě (0 nebo 1)
int target = 0;            // začátek cílové zóny
int width = 10;            // šířka zóny (s levelem se zmenšuje z 10 na 2)
unsigned long holdMs = 1000;   // jak dlouho je potřeba vydržet v zóně
unsigned long onSince = 0;     // od kdy je tečka v zóně (0 = není)
float x, y, z;             // hodnoty zrychlení/náklonu
float cursorPos = 32;      // vyhlazená poloha červené diody
String line;               // příkaz přicházející z počítače

// nová náhodná zóna podle levelu
void newTarget() {
  width  = max(2, 10 - (level - 1));          // level 1 = 10 diod ... level 9+ = 2 diody
  holdMs = max(300, 1000 - (level - 1) * 80); // vyšší level = kratší držení
  target = random(0, NUM_LEDS - width + 1);
  onSince = 0;
}

void handleCommand(String s) {
  s.trim();
  s.toUpperCase();
  if (s == "START") {
    running = true; player = 0; level = 1; newTarget();
    np.neoPixelFill(0, 60, 0, true); delay(200);
  } else if (s == "END") {
    running = false;
    np.neoPixelFill(60, 60, 60, true); delay(400);
  } else if (s.startsWith("LEVEL")) {
    level = constrain(s.substring(5).toInt(), 1, 10);
    newTarget();
  } else if (s.startsWith("MODE")) {
    if (s.indexOf("VERSUS") > 0) mode = 1;
    else if (s.indexOf("COOP") > 0) mode = 2;
    else mode = 0;
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  if (!IMU.begin()) {          // pokud selže akcelerometr, program se zastaví
    Serial.println("Failed to initialize IMU!");
    while (1);
  }
  randomSeed(analogRead(A0));
  newTarget();
}

void loop() {
  // 1) příkazy ze stránky
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n') { handleCommand(line); line = ""; }
    else if (c != '\r') line += c;
  }

  // 2) náklon -> poloha červené diody (-0.5 g až +0.5 g = dioda 0 až 63, tj. cca -30 až +30 stupňů)
  if (IMU.accelerationAvailable()) {
    IMU.readAcceleration(x, y, z);
  }
  float pos = (constrain(x, -0.5, 0.5) + 0.5) * (NUM_LEDS - 1);
  cursorPos = cursorPos * 0.7 + pos * 0.3;       // lehké vyhlazení
  int hodnota = (int)(cursorPos + 0.5);

  // 3) vykreslení (bez show po každé diodě, jen jednou na konci = rychlejší)
  for (int i = 0; i < NUM_LEDS; i++) {
    if (i == hodnota) np.neoPixelSetValue(i, 120, 0, 0, false);                       // červená = náklon
    else if (running && i >= target && i < target + width)
      np.neoPixelSetValue(i, ZR[player], ZG[player], ZB[player], false);              // cílová zóna
    else np.neoPixelSetValue(i, 0, 0, 20, false);                                     // zbytek modře
  }
  np.neoPixelShow();

  // 4) hra: vydržet v zóně holdMs = trefa
  if (running) {
    if (hodnota >= target && hodnota < target + width) {
      if (onSince == 0) onSince = millis();
      if (millis() - onSince >= holdMs) {
        Serial.print("HIT ");
        Serial.println(mode == 0 ? 1 : player + 1);
        np.neoPixelFill(ZR[player] * 3, ZG[player] * 3, ZB[player] * 3, true);   // zábleskem potvrdí trefu
        delay(150);
        if (mode != 0) player = 1 - player;      // ve dvou se hráči střídají
        newTarget();
      }
    } else {
      onSince = 0;
    }
  }

  delay(10);
}
