#include <NeoPixelConnect.h> // knihovna pro led pásku
#include <Arduino_LSM6DSOX.h> // knihovna pro čtení náklonu
#include <SPI.h>

#define MAXIMUM_NUM_NEOPIXELS 64 // Nastavení délky pásku na 64 diod


NeoPixelConnect np(4, MAXIMUM_NUM_NEOPIXELS, pio0, 0); // Inicializace pásku na digitálním pinu D4
int i;
int randNumber;
int citac = 0; // Počítadlo času stráveného v cíli
float x, y, z; // Proměnné pro ukládání hodnot zrychlení/náklonu
char ssid[] = "RP2040";        // your network SSID (name)
char pass[] = "ArduinoWeb";    // your network password (use for WPA, or use as key for WEP)
int keyIndex = 0;              // your network key index number (needed only for WEP)





void setup(){

  Serial.begin(115200); // linka pro PC konzoli
  delay(2000);
  Serial.println("In setup");
  i = 0;
  if (!IMU.begin()) { // Inicializuje vestavěný akcelerometr LSM6DSOX. Pokud selže, program se zastaví v nekonečné smyčce
    Serial.println("Failed to initialize IMU!");
    while (1);
  }

  Serial.print("Accelerometer sample rate = ");
  Serial.print(IMU.accelerationSampleRate());
  Serial.println("Hz");
  Serial.println();

 
  randNumber = random(0, 49); // Vygeneruje první náhodnou pozici cíle (od LED 0 do max 48, aby se zóna o šířce 10 LED vešla do rozsahu 64)

  // wait 10 seconds for connection:
  delay(1000);

  
}

void loop(){

    // set all LEDs to red, then green, and then blue
    //np.neoPixelFill(255, 0, 0, true);
    //delay(1000);
    //np.neoPixelClear(true);
    //delay(1000);

  
  //np.neoPixelSetValue(1,0,200,0,true);
  //np.neoPixelSetValue(2,200,200,0,true);
  //np.neoPixelSetValue(3,0,0,200,true);
  //np.neoPixelSetValue(4,200,0,200,true);

  
  if (IMU.accelerationAvailable()) {
    IMU.readAcceleration(x, y, z); // načte aktuální náklon

    //Serial.println("Accelerometer data: ");
    //Serial.print(Ax);
    //Serial.println();
  }

  int hodnota = mapFloat(x, -0.5, 0.5, 0, 63); // převede náklon v ose X (rozsah -0.5 g až +0.5 g) na index diody (0 až 63) => vrátí index v rozsahu 0-63 pro daný náklon x, odpovídá - 30 stupňů až +30 stupňů náklonu

   //np.neoPixelClear();
   
   // náklon mimo rozsah bude igorovat a setovat na maximální resp. minimální
  if(hodnota > 63) hodnota = 63;
  if(hodnota < 0) hodnota = 0;

//projde všechny diody v pásku
  for(int i=0;i<64;i++)
  {
    if(i==hodnota) np.neoPixelSetValue((uint8_t)i,120,0,0,true); // když se daný index rovná indexu vypočítanému z náklonu, daná dioda se vykreslí červeně
    else if(i>=randNumber && i< randNumber+10) np.neoPixelSetValue((uint8_t)i,20,20,0,true); // rozsah o velikosti 10 daný náhodným číslem  se vykreslí zeleně
    else np.neoPixelSetValue((uint8_t)i,0,0,20,true); // zbytek modře
  }
 
  if(hodnota>=randNumber && hodnota< randNumber+10){
citac++; // pokud je při tomto průchodu cyklem kulička v daném rozsahu, přičtě se hodnota čítače
  } else {
    citac = 0;
  }

  if(citac>50) { // splnění úkolu: 25 * delay 10ms => 2,5s
    randNumber = random(0, 49); // vygenerování nového rozsahu
    citac = 0; // vynulování čítače
  }

  //i++;
  //if(i == 24)
  //{
  //  i = 0;
  //  np.neoPixelClear();
  //}

  
 

  delay(10);
   
}


// převodník intervalu z in_min až in_max na out_min až out_max
int mapFloat(float x, float in_min, float in_max, int out_min, int out_max) { 
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

