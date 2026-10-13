#include <NeoPixelConnect.h>   // knihovna pro LED pásku
#include <Arduino_LSM6DSOX.h>  // knihovna pro čtení náklonu
#include <SPI.h>

#define MAXIMUM_NUM_NEOPIXELS 64   // délka pásku
#define SIRKA_CILE            10   // šířka cílové zóny (počet LED)
#define CILU_NA_LEVEL          3   // kolikrát musíme trefit cíl, než postoupíme do dalšího levelu
#define DOBA_V_CILI         5000UL // jak dlouho (ms) musí kulička vydržet v cíli
#define POCET_LEVELU           6   // počet levelů (poslední = červený = finální)

NeoPixelConnect np(4, MAXIMUM_NUM_NEOPIXELS, pio0, 0); // pásek na pinu D4

// Barva pozadí pro každý level: od klidné modré přes světlejší modré až po červenou
// (hodnoty R, G, B – nízký jas, ať to nesvítí do očí)
const uint8_t barvyPozadi[POCET_LEVELU][3] = {
  {  0,  0, 30 },  // level 1 – modrá (klid)
  {  0, 10, 30 },  // level 2 – světlejší modrá
  {  0, 22, 30 },  // level 3 – světle modrá / azurová
  { 20,  0, 25 },  // level 4 – fialová (přechod do teplých barev)
  { 30, 10,  0 },  // level 5 – oranžová
  { 30,  0,  0 }   // level 6 – červená (finální level)
};

// Náchylnost kuličky k náklonu: čím menší rozsah, tím menší náklon stačí
// k přejetí přes celý pásek => kulička je citlivější a "rychlejší"
// (hodnoty v g: 0.5 g ≈ 30°, 0.16 g ≈ 9°)
const float rozsahNaklonu[POCET_LEVELU] = { 0.50, 0.42, 0.35, 0.28, 0.22, 0.16 };

int level = 0;               // aktuální level (0 = první)
int splnenoCilu = 0;         // kolik cílů jsme v tomto levelu už splnili
int randNumber;              // začátek cílové zóny
bool vCili = false;          // je kulička právě v cíli?
unsigned long casVstupu = 0; // kdy kulička do cíle vjela (millis)
float x, y, z;               // hodnoty zrychlení/náklonu


// převodník intervalu z in_min až in_max na out_min až out_max
int mapFloat(float x, float in_min, float in_max, int out_min, int out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

// vygeneruje novou pozici cíle tak, aby kulička nestála hned v něm
void novyCil(int hodnota) {
  do {
    randNumber = random(0, MAXIMUM_NUM_NEOPIXELS - SIRKA_CILE + 1); // 0 až 54
  } while (hodnota >= randNumber - 5 && hodnota < randNumber + SIRKA_CILE + 5);
}

// konec hry – celý pásek bliká zeleně
void konecHry() {
  Serial.println("HOTOVO! Vsechny levely splneny.");
  while (true) {
    np.neoPixelFill(0, 60, 0, true);
    delay(400);
    np.neoPixelClear(true);
    delay(400);
  }
}

// zavolá se, když kulička vydrží v cíli požadovanou dobu
void cilSplnen(int hodnota) {
  vCili = false;
  splnenoCilu++;
  Serial.print("Cil splnen: ");
  Serial.print(splnenoCilu);
  Serial.print("/");
  Serial.println(CILU_NA_LEVEL);

  if (splnenoCilu >= CILU_NA_LEVEL) {
    splnenoCilu = 0;

    if (level == POCET_LEVELU - 1) {
      konecHry(); // dokončen červený (finální) level
    }

    level++;
    Serial.print("Postup do levelu ");
    Serial.println(level + 1);

    // krátce ukážeme novou barvu levelu přes celý pásek
    np.neoPixelFill(barvyPozadi[level][0], barvyPozadi[level][1], barvyPozadi[level][2], true);
    delay(1000);
  }

  novyCil(hodnota); // cíl se přesune jinam
}


void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("In setup");

  if (!IMU.begin()) { // když se akcelerometr nespustí, program se zastaví
    Serial.println("Failed to initialize IMU!");
    while (1);
  }

  Serial.print("Accelerometer sample rate = ");
  Serial.print(IMU.accelerationSampleRate());
  Serial.println("Hz");

  randomSeed(analogRead(A0)); // aby byly pozice cíle při každém spuštění jiné
  novyCil(MAXIMUM_NUM_NEOPIXELS / 2);
}


void loop() {
  if (IMU.accelerationAvailable()) {
    IMU.readAcceleration(x, y, z); // načte aktuální náklon
  }

  // převod náklonu na index diody – rozsah závisí na levelu (náchylnost)
  float r = rozsahNaklonu[level];
  int hodnota = mapFloat(x, -r, r, 0, MAXIMUM_NUM_NEOPIXELS - 1);

  // náklon mimo rozsah se ořízne na kraj pásku
  if (hodnota > MAXIMUM_NUM_NEOPIXELS - 1) hodnota = MAXIMUM_NUM_NEOPIXELS - 1;
  if (hodnota < 0) hodnota = 0;

  // vykreslení – hodnoty se jen nastaví (false) a pošlou na pásek najednou na konci
  for (int i = 0; i < MAXIMUM_NUM_NEOPIXELS; i++) {
    if (i == hodnota)
      np.neoPixelSetValue((uint8_t)i, 80, 80, 80, false);  // kulička bíle (na červeném pozadí by červená nebyla vidět)
    else if (i >= randNumber && i < randNumber + SIRKA_CILE)
      np.neoPixelSetValue((uint8_t)i, 0, 30, 0, false);    // cíl zeleně
    else
      np.neoPixelSetValue((uint8_t)i, barvyPozadi[level][0], barvyPozadi[level][1], barvyPozadi[level][2], false); // pozadí podle levelu
  }
  np.neoPixelShow();

  // měření času v cíli pomocí millis() (přesné, nezávisí na rychlosti smyčky)
  bool kulickaVCili = (hodnota >= randNumber && hodnota < randNumber + SIRKA_CILE);

  if (kulickaVCili) {
    if (!vCili) {
      vCili = true;
      casVstupu = millis();
    } else if (millis() - casVstupu >= DOBA_V_CILI) {
      cilSplnen(hodnota);
    }
  } else {
    vCili = false; // kulička vyjela z cíle => čas se nuluje
  }

  delay(10);
}