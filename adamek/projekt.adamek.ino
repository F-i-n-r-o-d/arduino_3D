#include <NeoPixelConnect.h> 
#include <Arduino_LSM6DSOX.h> 


#define MAXIMUM_NUM_NEOPIXELS 64 
#define POCET_BODU            4   
#define SIRKA_ZONY            10   
#define DRZENI_MS             500  
#define MIN_VZDALENOST        15  
#define PAUZA_VYSLEDEK_MS     4000 

NeoPixelConnect np(4, MAXIMUM_NUM_NEOPIXELS, pio0, 0); 

float x, y, z;
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


int mapFloat(float x, float in_min, float in_max, int out_min, int out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
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
  if (IMU.accelerationAvailable()) {
    IMU.readAcceleration(x, y, z); 
  }

  
  int hodnota = mapFloat(x, -0.5, 0.5, 0, 63);

  // náklon mimo rozsah se ořízne na minimum / maximum
  if (hodnota > 63) hodnota = 63;
  if (hodnota < 0) hodnota = 0;

  bool posledni = (cislo_bodu == POCET_BODU - 1); 
  bool v_zone = (hodnota >= zona && hodnota < zona + SIRKA_ZONY);

  // vykreslení pásku
  for (int i = 0; i < MAXIMUM_NUM_NEOPIXELS; i++) {
    if (i == hodnota) {
      np.neoPixelSetValue((uint8_t)i, 120, 0, 0, false);            // kulička = červená
    } else if (i >= zona && i < zona + SIRKA_ZONY) {
      if (posledni) np.neoPixelSetValue((uint8_t)i, 30, 20, 0, false); // cílová zóna = žlutá
      else          np.neoPixelSetValue((uint8_t)i, 0, 20, 0, false);  // kontrolní bod = zelená
    } else {
      np.neoPixelSetValue((uint8_t)i, 0, 0, 20, false);             // zbytek = modrá
    }
  }
  np.neoPixelShow(); 

 
  if (v_zone) {
    if (vstup_do_zony == 0) vstup_do_zony = millis(); 
    if (millis() - vstup_do_zony >= DRZENI_MS) {
      // bod splněn
      cislo_bodu++;
      vstup_do_zony = 0;
      if (cislo_bodu >= POCET_BODU) {
        konecKola();                  // splněna cílová zóna -> konec kola
      } else {
        zona = novaZona(zona);       
      }
    }
  } else {
    vstup_do_zony = 0;
  }

  delay(10);
}