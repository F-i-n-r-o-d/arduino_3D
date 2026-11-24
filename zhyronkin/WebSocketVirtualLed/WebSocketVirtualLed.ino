#include <NeoPixelConnect.h>
#include <Arduino_LSM6DSOX.h>
#include <WiFiNINA.h>
#include <SPI.h>

#define MAXIMUM_NUM_NEOPIXELS 64

NeoPixelConnect np(4, MAXIMUM_NUM_NEOPIXELS, pio0, 0);

// Nastavení pohybu
unsigned long intervalPohybu = 1000;   // slider, teď slouží jako citlivost

float poloha = 32.0;        // přesná poloha bodu (0-63)
float rychlost = 0.0;       // rychlost bodu v LED/s
unsigned long posledniCas = 0;

int polohaKulicky = 32;
int randNumber;
float casVCili = 0.0;       // sekundy strávené v cíli při správné rychlosti

float x = 0, y = 0, z = 0;
float gx = 0, gy = 0, gz = 0;   // gyroskop (°/s)

// Ladění fyziky
const int   SMER = 1;               // změň na -1, pokud se bod hýbe opačně
const float GYRO_PRAH = 5.0;        // mrtvá zóna (°/s), potlačí šum
const float ZISK = 0.6;             // LED/s na 1 °/s
const float MAX_RYCHLOST = 60.0;    // max rychlost bodu (LED/s)
const float UTLUM_ODRAZU = 0.6;     // kolik rychlosti zůstane po odrazu
const float TAU = 0.12;             // vyhlazení (s) - plynulost pohybu
const float LIMIT_V_CILI = 15.0;    // v cíli se počítá jen pomalejší pohyb

char ssid[] = "RP2040";
char pass[] = "ArduinoWeb";

WiFiServer server(80);

void setup() {
  Serial.begin(115200);
  delay(1500);

  randomSeed(analogRead(A0));
  randNumber = random(0, 54);

  if (!IMU.begin()) {
    Serial.println("Chyba IMU!");
    while (1);
  }

  Serial.print("Vytvarim AP: ");
  Serial.println(ssid);

  if (WiFi.beginAP(ssid, pass) != WL_AP_LISTENING) {
    Serial.println("Chyba AP!");
    while (1);
  }

  delay(2000);

  server.begin();

  Serial.print("Web server bezi na http://");
  Serial.println(WiFi.localIP());

  randNumber = random(0, 54);
  posledniCas = millis();
}

void loop() {

  // 1. Čtení senzorů
  if (IMU.accelerationAvailable()) {
    IMU.readAcceleration(x, y, z);
  }
  if (IMU.gyroscopeAvailable()) {
    IMU.readGyroscope(gx, gy, gz);
  }

  // 2. Čas
  unsigned long aktualniCas = millis();
  float dt = (aktualniCas - posledniCas) / 1000.0;
  posledniCas = aktualniCas;
  if (dt > 0.1) dt = 0.1;

  // 3. Rychlost tyče -> cílová rychlost bodu
  float omega = SMER * gy;                    // pokud tyč otáčíš kolem jiné osy, zkus gx / gz
  if (fabs(omega) < GYRO_PRAH) omega = 0;

  float citlivost = 1000.0 / intervalPohybu;  // slider: 300 ms = 3.3x, 3000 ms = 0.33x
  float cil = constrain(omega * ZISK * citlivost, -MAX_RYCHLOST, MAX_RYCHLOST);

  // Odraz při prudké změně směru tyče
  if (cil * rychlost < 0 && fabs(rychlost) > 8 && fabs(cil) > 8) {
    rychlost = -rychlost * UTLUM_ODRAZU;
  }

  // Plynulé dotažení rychlosti k cílové (trhavý pohyb se vyhladí)
  float alpha = dt / (TAU + dt);
  rychlost += (cil - rychlost) * alpha;

  // 4. Pohyb a odraz od konců pásku
  poloha += rychlost * dt;

  if (poloha < 0) {
    poloha = 0;
    rychlost = -rychlost * UTLUM_ODRAZU;
  }
  if (poloha > 63) {
    poloha = 63;
    rychlost = -rychlost * UTLUM_ODRAZU;
  }

  polohaKulicky = (int)(poloha + 0.5);

  bool vCili = (polohaKulicky >= randNumber && polohaKulicky < randNumber + 10);
  bool rychlostOk = fabs(rychlost) <= LIMIT_V_CILI;

  // 5. Vykreslení LED pásku (zobrazí se najednou na konci)
  for (int i = 0; i < 64; i++) {
    if (i == polohaKulicky) {
      if (rychlostOk) np.neoPixelSetValue((uint8_t)i, 0, 120, 0, false);   // zeleně = správná rychlost
      else            np.neoPixelSetValue((uint8_t)i, 120, 0, 0, false);   // červeně = moc rychle
    }
    else if (i >= randNumber && i < randNumber + 10) {
      np.neoPixelSetValue((uint8_t)i, 20, 20, 0, false);
    }
    else {
      np.neoPixelSetValue((uint8_t)i, 0, 0, 10, false);
    }
  }
  np.neoPixelShow();

  // 6. Herní logika: body se počítají jen při správné poloze I rychlosti
  if (vCili && rychlostOk) {
    casVCili += dt;
  }

  if (casVCili > 1.5) {
    randNumber = random(0, 54);
    casVCili = 0;
  }

  // 7. Web server
  WiFiClient client = server.available();

  if (client) {

    String currentLine = "";
    String requestLine = "";

    bool isDataRequest = false;
    bool isIntervalRequest = false;

    while (client.connected()) {

      if (client.available()) {

        char c = client.read();

        if (c == '\n') {

          // Prvni radek HTTP pozadavku
          if (requestLine.length() == 0) {
            requestLine = currentLine;

            // Telemetrie
            if (requestLine.startsWith("GET /data ")) {
              isDataRequest = true;
            }

            // Nastaveni intervalu (citlivosti) ze slideru
            if (requestLine.startsWith("GET /setInterval?value=")) {

              int zacatek = requestLine.indexOf("value=") + 6;
              int konec = requestLine.indexOf(' ', zacatek);

              String hodnotaText =
                requestLine.substring(zacatek, konec);

              int novaHodnota = hodnotaText.toInt();

              if (novaHodnota >= 300 && novaHodnota <= 3000) {
                intervalPohybu = novaHodnota;

                Serial.print("Novy interval: ");
                Serial.println(intervalPohybu);
              }

              isIntervalRequest = true;
            }
          }

          if (currentLine.length() == 0) {

            // Odpoved na pozadavek
            if (isDataRequest) {

              client.println("HTTP/1.1 200 OK");
              client.println("Content-Type: text/plain");
              client.println("Access-Control-Allow-Origin: *");
              client.println("Connection: close");
              client.println();

              client.print(x, 2);
              client.print(",");
              client.print(y, 2);
              client.print(",");
              client.print(z, 2);
              client.print(",");
              client.print(polohaKulicky);
              client.print(",");
              client.println(randNumber);

            }

            else if (isIntervalRequest) {

              client.println("HTTP/1.1 200 OK");
              client.println("Content-Type: text/plain");
              client.println("Access-Control-Allow-Origin: *");
              client.println("Connection: close");
              client.println();

              client.println("OK");
            }

            else {

              // HTML stranka
              client.println("HTTP/1.1 200 OK");
              client.println("Content-Type: text/html; charset=UTF-8");
              client.println("Connection: close");
              client.println();

              client.println(F("<!DOCTYPE html><html><head><meta charset='UTF-8'><title>Rehabko Monitor</title>"));

              client.println(F("<style>"));
              client.println(F("body{font-family:Segoe UI,sans-serif;background:#181818;color:#eee;text-align:center;padding:20px;}"));
              client.println(F(".track{width:640px;height:46px;background:#0d1117;margin:35px auto;border-radius:23px;position:relative;border:2px solid #30363d;box-shadow:inset 0 2px 6px #000;}"));
              client.println(F("#target{position:absolute;height:100%;background:#e3b341;opacity:0.65;border-radius:8px;}"));
              client.println(F("#ball{position:absolute;width:34px;height:34px;background:#f85149;border-radius:50%;top:6px;box-shadow:0 0 12px #f85149;transition:left 0.04s ease-out;}"));
              client.println(F(".card{background:#21262d;display:inline-block;padding:15px 30px;border-radius:10px;margin:10px;border:1px solid #30363d;}"));
              client.println(F("h1{margin-bottom:5px;} b{color:#58a6ff;font-size:1.3em;}"));
              client.println(F("</style></head><body>"));

              client.println(F("<h1>Rehabilitacni pomucka</h1><p>Zivy monitor naklonu</p>"));

              client.println(F("<div class='track'><div id='target'></div><div id='ball'></div></div>"));

              client.println(F("<div class='card'><p>Naklon X:<br><b id='valX'>0.00</b> g</p></div>"));

              client.println(F("<div class='card'><p>Poloha diody:<br><b id='valD'>0</b> / 63</p></div>"));

              client.println(F("<div class='card'>"));
              client.println(F("<p>Citlivost (nizsi = citlivejsi):</p>"));
              client.println(F("<input type='range' id='intervalSlider' min='300' max='3000' value='1000' step='100' style='width:300px;'>"));
              client.println(F("<p><b id='intervalValue'>1000</b></p>"));
              client.println(F("</div>"));

              // JavaScript
              client.println(F("<script>"));

              client.println(F("function loopData(){"));
              client.println(F("fetch('/data').then(r=>r.text()).then(txt=>{"));
              client.println(F("let d=txt.split(',');"));
              client.println(F("document.getElementById('valX').innerText=d[0];"));
              client.println(F("document.getElementById('valD').innerText=d[3];"));
              client.println(F("document.getElementById('ball').style.left=(parseFloat(d[3])*9.5)+'px';"));
              client.println(F("document.getElementById('target').style.left=(parseFloat(d[4])*10)+'px';"));
              client.println(F("document.getElementById('target').style.width='100px';"));
              client.println(F("}).catch(()=>{}).finally(()=>{setTimeout(loopData,35);});"));
              client.println(F("}"));

              // Slider -> Arduino
              client.println(F("document.getElementById('intervalSlider').addEventListener('input',function(){"));
              client.println(F("let hodnota=this.value;"));
              client.println(F("document.getElementById('intervalValue').innerText=hodnota;"));
              client.println(F("fetch('/setInterval?value='+hodnota).catch(error=>console.log(error));"));
              client.println(F("});"));

              client.println(F("loopData();"));

              client.println(F("</script></body></html>"));
            }

            break;

          } else {
            currentLine = "";
          }

        } else if (c != '\r') {
          currentLine += c;
        }
      }
    }

    client.stop();
  }
}