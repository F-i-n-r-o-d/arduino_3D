#include <NeoPixelConnect.h>
#include <Arduino_LSM6DSOX.h>
#include <WiFiNINA.h>
#include <SPI.h>

#define MAXIMUM_NUM_NEOPIXELS 64

// ==================================================
// LED PÁSEK
// ==================================================

NeoPixelConnect np(4, MAXIMUM_NUM_NEOPIXELS, pio0, 0);


// ==================================================
// WI-FI
// ==================================================

char ssid[] = "RP2040";
char pass[] = "ArduinoWeb";

WiFiServer server(80);


// ==================================================
// AKCELEROMETR
// ==================================================

float x, y, z;


// ==================================================
// HERNÍ PROMĚNNÉ
// ==================================================

// Pozice kuličky
float poziceKulicky = 0.0;

// Rychlost kuličky
float rychlostKulicky = 0.0;

// Aktuální level
int level = 1;

// Počet aktivních LED
int aktivniLED = 24;

// Pozice cíle
int poziceCile = 0;

// Velikost cíle
int velikostCile = 10;

// Počítadlo času v cíli
int citac = 0;


// ==================================================
// FYZIKA KULIČKY
// ==================================================

// Síla náklonu
float silaNaklonu = 0.035;

// Tření / zpomalování
float treni = 0.96;

// Maximální rychlost
float maximalniRychlost = 0.8;


// ==================================================
// NASTAVENÍ LEVELU
// ==================================================

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


  // Kulička začne znovu na začátku

  poziceKulicky = 0.0;

  rychlostKulicky = 0.0;

  citac = 0;


  // Informace do Serial Monitoru

  Serial.println();
  Serial.println("==============================");

  Serial.print("LEVEL: ");
  Serial.println(level);

  Serial.print("Aktivni LED: ");
  Serial.println(aktivniLED);

  Serial.print("Velikost cile: ");
  Serial.println(velikostCile);

  Serial.print("Pozice cile: ");
  Serial.println(poziceCile);

  Serial.println("==============================");
}


// ==================================================
// SETUP
// ==================================================

void setup() {

  Serial.begin(115200);

  delay(1500);


  // ------------------------------------------------
  // Inicializace IMU
  // ------------------------------------------------

  if (!IMU.begin()) {

    Serial.println("Chyba IMU!");

    while (1);
  }


  Serial.print("Accelerometer sample rate = ");
  Serial.print(IMU.accelerationSampleRate());
  Serial.println(" Hz");


  // ------------------------------------------------
  // Inicializace random
  // ------------------------------------------------

  randomSeed(micros());


  // ------------------------------------------------
  // První level
  // ------------------------------------------------

  nastavLevel();


  // ------------------------------------------------
  // Wi-Fi Access Point
  // ------------------------------------------------

  Serial.print("Vytvarim AP: ");
  Serial.println(ssid);


  if (WiFi.beginAP(ssid, pass) != WL_AP_LISTENING) {

    Serial.println("Chyba AP!");

    while (1);
  }


  delay(2000);


  // ------------------------------------------------
  // Web server
  // ------------------------------------------------

  server.begin();


  Serial.print("Web server bezi na http://");
  Serial.println(WiFi.localIP());
}


// ==================================================
// LOOP
// ==================================================

void loop() {

  // =================================================
  // 1. ČTENÍ AKCELEROMETRU
  // =================================================

  if (IMU.accelerationAvailable()) {

    IMU.readAcceleration(x, y, z);
  }


  // =================================================
  // 2. FYZIKA KULIČKY
  // =================================================

  /*
    Náklon zařízení způsobí zrychlení.

    Kulička tedy nereaguje okamžitě na změnu náklonu,
    ale postupně získává rychlost.
  */

  float zrychleni = x * silaNaklonu;


  // Přidání zrychlení

  rychlostKulicky += zrychleni;


  // Tření

  rychlostKulicky *= treni;


  // Omezení maximální rychlosti

  if (rychlostKulicky > maximalniRychlost) {
    rychlostKulicky = maximalniRychlost;
  }

  if (rychlostKulicky < -maximalniRychlost) {
    rychlostKulicky = -maximalniRychlost;
  }


  // Pohyb kuličky

  poziceKulicky += rychlostKulicky;


  // =================================================
  // 3. ODRAZ OD LEVÉHO OKRAJE
  // =================================================

  if (poziceKulicky < 0) {

    poziceKulicky = 0;

    rychlostKulicky =
      -rychlostKulicky * 0.8;
  }


  // =================================================
  // 4. ODRAZ OD PRAVÉHO OKRAJE
  // =================================================

  if (poziceKulicky > aktivniLED - 1) {

    poziceKulicky = aktivniLED - 1;

    rychlostKulicky =
      -rychlostKulicky * 0.8;
  }


  // =================================================
  // 5. PŘEVOD POZICE NA LED
  // =================================================

  int hodnota = round(poziceKulicky);


  if (hodnota < 0) {
    hodnota = 0;
  }

  if (hodnota >= aktivniLED) {
    hodnota = aktivniLED - 1;
  }


  // =================================================
  // 6. VYKRESLENÍ FYZICKÉ LED PÁSKY
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

    if (i == hodnota) {

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
        10,
        true
      );
    }
  }


  // =================================================
  // 7. KONTROLA CÍLE
  // =================================================

  if (
    poziceKulicky >= poziceCile &&
    poziceKulicky < poziceCile + velikostCile
  ) {

    citac++;

  } else {

    citac = 0;
  }


  // =================================================
  // 8. SPLNĚNÍ LEVELU
  // =================================================

  /*
    Loop běží přibližně každých 10 ms.

    150 průchodů ≈ 1,5 sekundy.

    Kulička musí být v cíli
    přibližně 1,5 sekundy.
  */

  if (citac > 150) {

    Serial.println("CIL SPLNEN!");

    level++;


    // Maximum level 6

    if (level > 6) {
      level = 6;
    }


    // Nastavení dalšího levelu

    nastavLevel();
  }


  // =================================================
  // 9. WEB SERVER
  // =================================================

  WiFiClient client = server.available();


  if (client) {

    String currentLine = "";

    bool isDataRequest = false;


    while (client.connected()) {

      if (client.available()) {

        char c = client.read();


        // ------------------------------------------------
        // Konec HTTP řádku
        // ------------------------------------------------

        if (c == '\n') {

          // Prázdný řádek = konec HTTP hlaviček

          if (currentLine.length() == 0) {


            // ============================================
            // /data
            // ============================================

            if (isDataRequest) {

              client.println("HTTP/1.1 200 OK");

              client.println(
                "Content-Type: text/plain"
              );

              client.println(
                "Access-Control-Allow-Origin: *"
              );

              client.println(
                "Connection: close"
              );

              client.println();


              /*
                Odesíláme:

                x
                y
                z
                pozice kuličky
                pozice cíle
                velikost cíle
                level
                aktivní LED
                progress
              */


              client.print(x, 2);
              client.print(",");

              client.print(y, 2);
              client.print(",");

              client.print(z, 2);
              client.print(",");

              client.print(poziceKulicky, 2);
              client.print(",");

              client.print(poziceCile);
              client.print(",");

              client.print(velikostCile);
              client.print(",");

              client.print(level);
              client.print(",");

              client.print(aktivniLED);
              client.print(",");

              client.println(citac);
            }


            // ============================================
            // HLAVNÍ WEBOVÁ STRÁNKA
            // ============================================

            else {

              client.println(
                "HTTP/1.1 200 OK"
              );

              client.println(
                "Content-Type: text/html; charset=UTF-8"
              );

              client.println(
                "Connection: close"
              );

              client.println();


              // ========================================
              // HTML + CSS
              // ========================================

              client.println(
                F(
                  "<!DOCTYPE html>"
                  "<html>"
                  "<head>"
                  "<meta charset='UTF-8'>"
                  "<meta name='viewport' content='width=device-width,initial-scale=1'>"
                  "<title>Smart Balance Labyrinth</title>"

                  "<style>"

                  "body{"
                  "font-family:Segoe UI,sans-serif;"
                  "background:#181818;"
                  "color:#eee;"
                  "text-align:center;"
                  "padding:20px;"
                  "}"

                  "h1{"
                  "margin-bottom:5px;"
                  "}"

                  ".subtitle{"
                  "color:#aaa;"
                  "}"

                  ".track{"
                  "width:640px;"
                  "max-width:90%;"
                  "height:46px;"
                  "background:#0d1117;"
                  "margin:35px auto;"
                  "border-radius:23px;"
                  "position:relative;"
                  "border:2px solid #30363d;"
                  "box-shadow:inset 0 2px 6px #000;"
                  "overflow:hidden;"
                  "}"

                  "#target{"
                  "position:absolute;"
                  "height:100%;"
                  "background:#e3b341;"
                  "opacity:0.65;"
                  "border-radius:8px;"
                  "}"

                  "#ball{"
                  "position:absolute;"
                  "width:34px;"
                  "height:34px;"
                  "background:#f85149;"
                  "border-radius:50%;"
                  "top:6px;"
                  "box-shadow:0 0 12px #f85149;"
                  "}"

                  ".card{"
                  "background:#21262d;"
                  "display:inline-block;"
                  "padding:15px 30px;"
                  "border-radius:10px;"
                  "margin:10px;"
                  "border:1px solid #30363d;"
                  "min-width:120px;"
                  "}"

                  "h2{"
                  "margin:5px;"
                  "color:#58a6ff;"
                  "}"

                  "b{"
                  "color:#58a6ff;"
                  "font-size:1.3em;"
                  "}"

                  ".progress{"
                  "width:500px;"
                  "max-width:80%;"
                  "height:15px;"
                  "background:#30363d;"
                  "margin:20px auto;"
                  "border-radius:10px;"
                  "overflow:hidden;"
                  "}"

                  "#progressBar{"
                  "height:100%;"
                  "width:0%;"
                  "background:#3fb950;"
                  "}"

                  "</style>"

                  "</head>"
                  "<body>"
                )
              );


              // ========================================
              // TITULEK
              // ========================================

              client.println(
                F(
                  "<h1>Smart Balance Labyrinth</h1>"
                  "<p class='subtitle'>"
                  "Rehabilitační pomůcka pro trénink rovnováhy"
                  "</p>"
                )
              );


              // ========================================
              // LEVEL
              // ========================================

              client.println(
                F(
                  "<div class='card'>"
                  "<p>LEVEL</p>"
                  "<h2 id='level'>1</h2>"
                  "</div>"
                )
              );


              // ========================================
              // AKTIVNÍ LED
              // ========================================

              client.println(
                F(
                  "<div class='card'>"
                  "<p>Aktivní LED</p>"
                  "<b id='active'>24</b> / 64"
                  "</div>"
                )
              );


              // ========================================
              // NÁKLON X
              // ========================================

              client.println(
                F(
                  "<div class='card'>"
                  "<p>Náklon X</p>"
                  "<b id='valX'>0.00</b> g"
                  "</div>"
                )
              );


              // ========================================
              // POZICE KULIČKY
              // ========================================

              client.println(
                F(
                  "<div class='card'>"
                  "<p>Poloha kuličky</p>"
                  "<b id='valD'>0.00</b>"
                  "</div>"
                )
              );


              // ========================================
              // VIRTUÁLNÍ PÁSEK
              // ========================================

              client.println(
                F(
                  "<div class='track'>"
                  "<div id='target'></div>"
                  "<div id='ball'></div>"
                  "</div>"
                )
              );


              // ========================================
              // PROGRESS
              // ========================================

              client.println(
                F(
                  "<p>Čas v cílové zóně</p>"
                  "<div class='progress'>"
                  "<div id='progressBar'></div>"
                  "</div>"
                )
              );


              // ========================================
              // JAVASCRIPT
              // ========================================

              client.println(
                F(
                  "<script>"

                  "function loopData(){"

                  "fetch('/data')"

                  ".then(r=>r.text())"

                  ".then(txt=>{"

                  "let d=txt.split(',');"


                  // X

                  "document.getElementById('valX').innerText=d[0];"


                  // Pozice kuličky

                  "let ball=parseFloat(d[3]);"


                  // Cíl

                  "let target=parseFloat(d[4]);"


                  // Velikost cíle

                  "let size=parseFloat(d[5]);"


                  // Level

                  "let lvl=parseInt(d[6]);"


                  // Aktivní LED

                  "let active=parseInt(d[7]);"


                  // Progress

                  "let progress=parseInt(d[8]);"


                  "document.getElementById('level').innerText=lvl;"

                  "document.getElementById('active').innerText=active;"

                  "document.getElementById('valD').innerText=ball.toFixed(2);"


                  // ==================================
                  // Pozice na virtuální liště
                  // ==================================

                  "let scale=640/63;"

                  "document.getElementById('ball').style.left="
                  "(ball*scale-17)+'px';"


                  "document.getElementById('target').style.left="
                  "(target*scale)+'px';"


                  "document.getElementById('target').style.width="
                  "(size*scale)+'px';"


                  // ==================================
                  // Progress
                  // ==================================

                  "let p=(progress/150)*100;"

                  "if(p>100)p=100;"

                  "document.getElementById('progressBar').style.width="
                  "p+'%';"

                  "})"

                  ".catch(()=>{})"

                  ".finally(()=>{"
                  "setTimeout(loopData,50);"
                  "});"

                  "}"

                  "loopData();"

                  "</script>"

                  "</body>"
                  "</html>"
                )
              );
            }

            break;
          }

          else {

            currentLine = "";
          }
        }


        // ------------------------------------------------
        // Čtení HTTP požadavku
        // ------------------------------------------------

        else if (c != '\r') {

          currentLine += c;


          if (currentLine.endsWith("GET /data")) {

            isDataRequest = true;
          }
        }
      }
    }


    client.stop();
  }


  // =================================================
  // MALÁ PAUZA
  // =================================================

  delay(10);
}