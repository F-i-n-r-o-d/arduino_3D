#include <NeoPixelConnect.h> // knihovna pro LED pásek
#include <Arduino_LSM6DSOX.h> // knihovna pro akcelerometr
#include <SPI.h>

#define MAXIMUM_NUM_NEOPIXELS 64

// --------------------------------------------------
// LED pásek
// --------------------------------------------------

NeoPixelConnect np(4, MAXIMUM_NUM_NEOPIXELS, pio0, 0);

// --------------------------------------------------
// Akcelerometr
// --------------------------------------------------

float x, y, z;

// --------------------------------------------------
// Herní proměnné
// --------------------------------------------------

float poziceKulicky = 0.0;
float rychlostKulicky = 0.0;

int level = 1;

// Počet aktivních LED v aktuálním levelu
int aktivniLED = 24;

// Pozice cíle
int poziceCile = 0;

// Velikost cíle
int velikostCile = 10;

// Počítadlo času v cíli
int citac = 0;

// --------------------------------------------------
// Fyzika
// --------------------------------------------------

// Síla náklonu
float silaNaklonu = 0.035;

// Zpomalení
float treni = 0.96;

// Maximální rychlost
float maximalniRychlost = 0.8;


// --------------------------------------------------
// Převod float intervalu
// --------------------------------------------------

int mapFloat(
  float hodnota,
  float in_min,
  float in_max,
  int out_min,
  int out_max
) {

  return (hodnota - in_min) *
         (out_max - out_min) /
         (in_max - in_min) +
         out_min;
}


// --------------------------------------------------
// Nastavení nového levelu
// --------------------------------------------------

void nastavLevel() {

  /*
    Level 1 = 24 LED
    Level 2 = 32 LED
    Level 3 = 40 LED
    Level 4 = 48 LED
    Level 5 = 56 LED
    Level 6 = 64 LED
  */

  aktivniLED = 24 + (level - 1) * 8;

  if (aktivniLED > 64) {
    aktivniLED = 64;
  }


  /*
    Velikost cíle:

    Level 1 = 10 LED
    Level 2 = 9 LED
    Level 3 = 8 LED
    Level 4 = 7 LED
    Level 5 = 6 LED
    Level 6 = 5 LED
  */

  velikostCile = 10 - (level - 1);

  if (velikostCile < 5) {
    velikostCile = 5;
  }


  // Náhodná pozice cíle

  poziceCile = random(
    0,
    aktivniLED - velikostCile + 1
  );


  // Kulička se vrátí na začátek

  poziceKulicky = 0;

  rychlostKulicky = 0;

  citac = 0;


  // Informace do Serial Monitoru

  Serial.println();
  Serial.println("-----------------------------");

  Serial.print("LEVEL: ");
  Serial.println(level);

  Serial.print("Aktivni LED: ");
  Serial.println(aktivniLED);

  Serial.print("Velikost cile: ");
  Serial.println(velikostCile);

  Serial.print("Pozice cile: ");
  Serial.println(poziceCile);

  Serial.println("-----------------------------");
}


// --------------------------------------------------
// Setup
// --------------------------------------------------

void setup() {

  Serial.begin(115200);

  delay(2000);

  Serial.println("In setup");


  // Inicializace akcelerometru

  if (!IMU.begin()) {

    Serial.println("Failed to initialize IMU!");

    while (1);
  }


  Serial.print("Accelerometer sample rate = ");
  Serial.print(IMU.accelerationSampleRate());
  Serial.println(" Hz");

  Serial.println();


  // Inicializace náhodného čísla

  randomSeed(micros());


  // Nastavení prvního levelu

  nastavLevel();
}


// --------------------------------------------------
// Hlavní program
// --------------------------------------------------

void loop() {

  // =================================================
  // 1. ČTENÍ AKCELEROMETRU
  // =================================================

  if (IMU.accelerationAvailable()) {

    IMU.readAcceleration(x, y, z);
  }


  // =================================================
  // 2. VÝPOČET POHYBU KULIČKY
  // =================================================

  /*
    X určuje směr pohybu.

    Náklon jedním směrem:
      → kulička zrychluje doprava

    Náklon opačným směrem:
      → kulička zrychluje doleva
  */

  float zrychleni = x * silaNaklonu;


  // Přidáme zrychlení k rychlosti

  rychlostKulicky += zrychleni;


  // Přirozené zpomalení

  rychlostKulicky *= treni;


  // Omezení rychlosti

  if (rychlostKulicky > maximalniRychlost) {
    rychlostKulicky = maximalniRychlost;
  }

  if (rychlostKulicky < -maximalniRychlost) {
    rychlostKulicky = -maximalniRychlost;
  }


  // Posun kuličky

  poziceKulicky += rychlostKulicky;


  // =================================================
  // 3. ODRAZ OD ZAČÁTKU
  // =================================================

  if (poziceKulicky < 0) {

    poziceKulicky = 0;

    // Odraz

    rychlostKulicky =
      -rychlostKulicky * 0.8;
  }


  // =================================================
  // 4. ODRAZ OD KONCE AKTIVNÍ ČÁSTI
  // =================================================

  if (poziceKulicky > aktivniLED - 1) {

    poziceKulicky = aktivniLED - 1;

    // Odraz

    rychlostKulicky =
      -rychlostKulicky * 0.8;
  }


  // =================================================
  // 5. PŘEVOD POZICE NA LED
  // =================================================

  int LEDKulicky = round(poziceKulicky);


  if (LEDKulicky < 0) {
    LEDKulicky = 0;
  }


  if (LEDKulicky >= aktivniLED) {
    LEDKulicky = aktivniLED - 1;
  }


  // =================================================
  // 6. VYKRESLENÍ LED
  // =================================================

  for (int i = 0; i < 64; i++) {

    // LED, které ještě nejsou odemčené

    if (i >= aktivniLED) {

      np.neoPixelSetValue(
        (uint8_t)i,
        0,
        0,
        0,
        true
      );

      continue;
    }


    // ------------------------------------------------
    // Červená kulička
    // ------------------------------------------------

    if (i == LEDKulicky) {

      np.neoPixelSetValue(
        (uint8_t)i,
        120,
        0,
        0,
        true
      );
    }


    // ------------------------------------------------
    // Žlutý cíl
    // ------------------------------------------------

    else if (
      i >= poziceCile &&
      i < poziceCile + velikostCile
    ) {

      np.neoPixelSetValue(
        (uint8_t)i,
        20,
        20,
        0,
        true
      );
    }


    // ------------------------------------------------
    // Modré pozadí
    // ------------------------------------------------

    else {

      np.neoPixelSetValue(
        (uint8_t)i,
        0,
        0,
        20,
        true
      );
    }
  }


  // =================================================
  // 7. KONTROLA, JESTLI JE KULIČKA V CÍLI
  // =================================================

  if (
    poziceKulicky >= poziceCile &&
    poziceKulicky < poziceCile + velikostCile
  ) {

    // Kulička je v cíli

    citac++;

  } else {

    // Kulička opustila cíl

    citac = 0;
  }


  // =================================================
  // 8. SPLNĚNÍ LEVELU
  // =================================================

  /*
    Loop má přibližně 10 ms.

    150 × 10 ms = přibližně 1,5 sekundy.

    Kulička tedy musí být v cíli
    přibližně 1,5 sekundy.
  */

  if (citac > 150) {

    Serial.println();
    Serial.println("CIL SPLNEN!");

    level++;


    // Maximum je level 6

    if (level > 6) {
      level = 6;
    }


    // Nastavení dalšího levelu

    nastavLevel();
  }


  // =================================================
  // 9. SERIAL MONITOR
  // =================================================

  /*
    Tyto hodnoty můžeš později použít
    také pro Wi-Fi/webové rozhraní.
  */

  /*
  Serial.print("X: ");
  Serial.print(x);

  Serial.print(" | Pozice: ");
  Serial.print(poziceKulicky);

  Serial.print(" | Rychlost: ");
  Serial.print(rychlostKulicky);

  Serial.print(" | Level: ");
  Serial.println(level);
  */


  // =================================================
  // 10. MALÁ PAUZA
  // =================================================

  delay(10);
}