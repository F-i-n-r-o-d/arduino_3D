#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <FastLED.h>
// --- Nastavení LED pásku ---
#define POCET_LED 60        // Uprav podle reálného počtu LED na liště
#define DATOVY_PIN 6        // Pin na Arduinu, kam vede datový vodič pásku
#define JAS_LED 60          // Jas pásku (0 až 255)
CRGB ledky[POCET_LED];
// --- Senzor náklonu ---
Adafruit_MPU6050 senzor;
// --- Herní parametry kuličky ---
float poziceKulicky = 0.0;
float rychlostKulicky = 0.0;
const float treni = 0.94;           // Tlumení pohybu (aby kulička nezrychlovala do nekonečna)
const float citlivostListy = 0.08;  // Jak prudce kulička reaguje na náklon lišty
// --- Herní parametry tlakové vlny (obrana) ---
int poziceVlny = -1;                // -1 znamená, že žádná vlna zrovna neletí
const int rychlostVlny = 2;         // Kolik LEDek vlna přeskočí za jeden cyklus
unsigned long casPosledniVlny = 0;  // Uložení času výstřelu
const unsigned long cooldownVlny = 2000; // 2 sekundy pauza mezi výstřely z klávesnice
// --- Časování smyčky ---
unsigned long casPoslednihoKroku = 0;
const int snimkovyInterval = 30;    // Obnova hry každých cca 30 ms (~33 FPS)
void setup() {
  Serial.begin(9600);
  // Inicializace adresovatelného LED pásku
  FastLED.addLeds<WS2812B, DATOVY_PIN, GRB>(ledky, POCET_LED);
  FastLED.setBrightness(JAS_LED);
  FastLED.clear();
  FastLED.show();
  // Inicializace gyroskopu / akcelerometru MPU6050
  if (!senzor.begin()) {
    // Pokud čidlo neodpovídá, rozsvítí se první LEDka červeně jako chyba
    ledky[0] = CRGB::Red;
    FastLED.show();
    while (1) {
      delay(10);
    }
  }
  // Nastavení rozsahů senzoru pro hladký chod
  senzor.setAccelerometerRange(MPU6050_RANGE_4_G);
  senzor.setFilterBandwidth(MPU6050_BAND_21_HZ);
}
void loop() {
  unsigned long aktualniCas = millis();
  // 1. Čtení klávesnice z PC přes Serial Monitor (stisknutí mezerníku obráncem)
  if (Serial.available() > 0) {
    char stisknutyZnak = Serial.read();
    // Reakce na mezerník, pokud už vypršel 2sekundový cooldown a vlna neletí
    if (stisknutyZnak == ' ') {
      if (aktualniCas - casPosledniVlny >= cooldownVlny && poziceVlny == -1) {
        poziceVlny = POCET_LED - 4; // Vlna vystartuje kousek před domečkem
        casPosledniVlny = aktualniCas;
      }
    }
  }
  // 2. Pravidelná herní smyčka
  if (aktualniCas - casPoslednihoKroku >= snimkovyInterval) {
    casPoslednihoKroku = aktualniCas;
    // --- Načtení náklonu z osy X nebo Y (podle natočení čidla na liště) ---
    sensors_event_t a, g, temp;
    senzor.getEvent(&a, &g, &temp);
    // a.acceleration.x udává gravitační sílu při naklonění tyče
    float naklon = a.acceleration.x;
    // --- Fyzika kuličky ---
    rychlostKulicky += naklon * citlivostListy;
    rychlostKulicky *= treni;
    poziceKulicky += rychlostKulicky;
    // Omezení začátku lišty (spodní doraz)
    if (poziceKulicky < 0) {
      poziceKulicky = 0;
      rychlostKulicky = 0;
    }
    // --- Pohyb a kolize tlakové vlny ---
    if (poziceVlny >= 0) {
      poziceVlny -= rychlostVlny; // Vlna letí zprava doleva
      // Zásah: pokud se vlna přiblíží ke kuličce na 2 LEDky
      if (abs(poziceVlny - (int)poziceKulicky) <= 2) {
        poziceKulicky -= 12;      // Tlaková vlna kuličku odfoukne o 12 pozic zpět
        rychlostKulicky = -2.5;   // Udělí kuličce silný zpětný ráz
        poziceVlny = -1;          // Vlna po nárazu zaniká
        if (poziceKulicky < 0) poziceKulicky = 0;
      } else if (poziceVlny < 0) {
        // Vlna doletěla až na úplný začátek a minula kuličku
        poziceVlny = -1;
      }
    }
    // --- Kontrola vítězství ---
    if (poziceKulicky >= POCET_LED - 3) {
      // Kulička úspěšně dorazila do domečku -> vítězná animace
      for (int i = 0; i < 3; i++) {
        fill_solid(ledky, POCET_LED, CRGB::Green);
        FastLED.show();
        delay(150);
        fill_solid(ledky, POCET_LED, CRGB::Black);
        FastLED.show();
        delay(150);
      }
      // Reset hry na začátek
      poziceKulicky = 0;
      rychlostKulicky = 0;
      poziceVlny = -1;
      return;
    }
    // --- Vykreslení celé scény na pásek ---
    FastLED.clear();
    // 1. Zelený domeček na konci pásku (poslední 3 LEDky)
    for (int i = POCET_LED - 3; i < POCET_LED; i++) {
      ledky[i] = CRGB::Green;
    }
    // 2. Modrá kulička (s ošetřením, aby index nikdy nepřetekl pole)
    int indexKulicky = constrain((int)poziceKulicky, 0, POCET_LED - 1);
    ledky[indexKulicky] = CRGB::DeepSkyBlue;
    // 3. Červená tlaková vlna (zobrazena jako 2 LEDky za sebou pro lepší viditelnost)
    if (poziceVlny >= 0) {
      if (poziceVlny < POCET_LED) ledky[poziceVlny] = CRGB::Red;
      if (poziceVlny + 1 < POCET_LED) ledky[poziceVlny + 1] = CRGB::DarkRed;
    }
    // 4. Indikátor cooldownu obranné vlny (úplně první LEDka na startu)
    // Svítí zeleně, když může obránce vystřelit, zhasne/zčervená během nabíjení
    if (aktualniCas - casPosledniVlny >= cooldownVlny) {
      ledky[0] = CRGB::DarkGreen;
    } else {
      ledky[0] = CRGB::DarkRed;
    }
    FastLED.show();
  }
}