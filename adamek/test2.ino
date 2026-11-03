#include <NeoPixelConnect.h>
#include <Arduino_LSM6DSOX.h>


#define MAXIMUM_NUM_NEOPIXELS 64
#define POCET_BODU            4
#define SIRKA_ZONY            10
#define DRZENI_MS             500
#define MIN_VZDALENOST        15
#define PAUZA_VYSLEDEK_MS     4000

// --- plynulost pohybu ---
#define VYHLAZENI_X   0.15f   // filtr surových dat z akcelerometru (0..1, menší = klidnější)
#define VYHLAZENI_POL 0.25f   // dojíždění tečky k cílové pozici (0..1, menší = plynulejší)
#define JAS_KULICKY   120

NeoPixelConnect np(4, MAXIMUM_NUM_NEOPIXELS, pio0, 0);

float x, y, z;
float x_filtr = 0;                // vyhlazená hodnota náklonu
float poloha = 31.5;              // plynulá (desetinná) poloha kuličky na pásku
int zona = 0;
int cislo_bodu = 0;
unsigned long start_kola = 0;
unsigned long vstup_do_zony = 0;
unsigned long nejlepsi_cas = 0;


int novaZona(int predchozi) {
  int nova;
  do {
    nova = random(0, MAXIMUM_NUM_NEOPIXELS - SIRKA_ZONY + 1);
  } while (predchozi >= 0 && abs(nova - predchozi) < MIN_VZDALENOST);
  return nova;
}


void noveKolo() {
  cislo_bodu = 0;
  vstup_do_zony = 0;
  zona = novaZona(-1);
  start_kola = millis();
  Serial.println("--- Nove kolo, cas bezi! ---");
}


void zablikej(uint8_t r, uint8_t g, uint8_t b, int pocet) {
  for (int k = 0; k < pocet; k++) {
    np.neoPixelFill(r, g, b, true);
    delay(300);
    np.neoPixelClear(true);
    delay(300);
  }
}


void konecKola() {
  unsigned long cas = millis() - start_kola;
  bool rekord = (nejlepsi_cas == 0 || cas < nejlepsi_cas);

  Serial.print("Cil! Vas cas: ");
  Serial.print(cas / 1000.0, 2);
  Serial.println(" s");

  if (rekord) {
    nejlepsi_cas = cas;
    Serial.println("NOVY REKORD!");
    zablikej(0, 120, 0, 5);     // zeleně = nový nejlepší čas
  } else {
    Serial.print("Nejlepsi cas: ");
    Serial.print(nejlepsi_cas / 1000.0, 2);
    Serial.println(" s");
    zablikej(120, 40, 0, 3);    // oranžově = horší než rekord
  }

  delay(PAUZA_VYSLEDEK_MS - (rekord ? 3000 : 1800));
  noveKolo();
}


// mapování s desetinným výsledkem (žádné useknutí na celé číslo)
float mapFloat(float v, float in_min, float in_max, float out_min, float out_max) {
  return (v - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}


// smíchá barvu pozadí s červenou podle váhy w (0..1)
uint8_t smichej(uint8_t pozadi, uint8_t cil, float w) {
  return (uint8_t)(pozadi + (cil - pozadi) * w + 0.5f);
}


void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("In setup");

  if (!IMU.begin()) {
    Serial.println("Failed to initialize IMU!");
    while (1);
  }

  Serial.print("Accelerometer sample rate = ");
  Serial.print(IMU.accelerationSampleRate());
  Serial.println("Hz");
  Serial.println();

  randomSeed(analogRead(A0));

  noveKolo();
}


void loop() {
  // 1) čtení a vyhlazení akcelerometru (odstraní šum = cukání tečky)
  if (IMU.accelerationAvailable()) {
    IMU.readAcceleration(x, y, z);
    x_filtr += VYHLAZENI_X * (x - x_filtr);
  }

  // 2) cílová poloha (desetinná) a plynulé dojíždění k ní
  float cil = mapFloat(x_filtr, -0.5, 0.5, 0, MAXIMUM_NUM_NEOPIXELS - 1);
  if (cil > MAXIMUM_NUM_NEOPIXELS - 1) cil = MAXIMUM_NUM_NEOPIXELS - 1;
  if (cil < 0) cil = 0;
  poloha += VYHLAZENI_POL * (cil - poloha);

  int hodnota = (int)(poloha + 0.5f);  // celočíselná pozice pro herní logiku

  bool posledni = (cislo_bodu == POCET_BODU - 1);
  bool v_zone = (hodnota >= zona && hodnota < zona + SIRKA_ZONY);

  // 3) vykreslení pásku – kulička se rozprostře mezi dvě sousední LED,
  //    takže se posouvá plynule a nepřeskakuje
  for (int i = 0; i < MAXIMUM_NUM_NEOPIXELS; i++) {
    uint8_t r, g, b;
    if (i >= zona && i < zona + SIRKA_ZONY) {
      if (posledni) { r = 30; g = 20; b = 0; }   // cílová zóna = žlutá
      else          { r = 0;  g = 20; b = 0; }   // kontrolní bod = zelená
    } else {
      r = 0; g = 0; b = 20;                      // zbytek = modrá
    }

    float w = 1.0f - fabs(i - poloha);           // jak moc tahle LED patří kuličce
    if (w > 0) {
      r = smichej(r, JAS_KULICKY, w);
      g = smichej(g, 0, w);
      b = smichej(b, 0, w);
    }

    np.neoPixelSetValue((uint8_t)i, r, g, b, false);
  }
  np.neoPixelShow();

  // 4) herní logika
  if (v_zone) {
    if (vstup_do_zony == 0) vstup_do_zony = millis();
    if (millis() - vstup_do_zony >= DRZENI_MS) {
      cislo_bodu++;
      vstup_do_zony = 0;
      if (cislo_bodu >= POCET_BODU) {
        konecKola();
      } else {
        zona = novaZona(zona);
      }
    }
  } else {
    vstup_do_zony = 0;
  }

  delay(10);
}
