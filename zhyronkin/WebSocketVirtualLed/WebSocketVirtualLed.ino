#include <NeoPixelConnect.h>
#include <Arduino_LSM6DSOX.h>
#include <WiFiNINA.h>
#include <SPI.h>

#define MAXIMUM_NUM_NEOPIXELS 64

NeoPixelConnect np(4, MAXIMUM_NUM_NEOPIXELS, pio0, 0);

int randNumber;
int citac = 0;
float x, y, z;
int posledniHodnota = -1;
unsigned long posledniCas = 0;
float rychlost = 0;
int smer = 0;
float odrazPozice = 0.0f;
float odrazRychlost = 0.0f;
int posledniSmer = 0;
int level = 1;
int score = 0;
int bestScore = 0;
int targetSize = 10;
float targetPos = 0.0f;
bool targetMovingRight = true;
char ssid[] = "[(67+67) + 6*7]/7 = xx,xxxxxx";        
char pass[] = "25,142857";    

WiFiServer server(80);

int mapFloat(float val, float in_min, float in_max, int out_min, int out_max) {
  return (val - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

void setup() {
  Serial.begin(115200);
  delay(1500);

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
  targetPos = randNumber;
}

void loop() {
  // 1. Cteni akcelerometru
  if (IMU.accelerationAvailable()) {
    IMU.readAcceleration(x, y, z);
  }

  int hodnota = mapFloat(x, -0.5, 0.5, 0, 63);
  if (hodnota > 63) hodnota = 63;
  if (hodnota < 0) hodnota = 0;

  // 2. Vypocet rychlosti a smeru pohybu
  unsigned long aktualniCas = millis();
  if (posledniHodnota == -1) {
    posledniHodnota = hodnota;
    posledniCas = aktualniCas;
  } else {
    unsigned long uplynulyCas = aktualniCas - posledniCas;
    if (uplynulyCas > 0) {
      int zmenaPolohy = hodnota - posledniHodnota;

      rychlost = abs(zmenaPolohy) * 1000.0 / uplynulyCas;
      if (zmenaPolohy > 0) {
        smer = 1;
      } else if (zmenaPolohy < 0) {
        smer = -1;
      } else {
        smer = 0;
      }

      posledniHodnota = hodnota;
      posledniCas = aktualniCas;
    }
  }

  // 3. Odrazovy efekt a pohyb cile
  float odrazSil = constrain(abs(rychlost) * 0.55f, 0.0f, 7.0f);
  if (smer == 0) {
    odrazRychlost *= 0.7f;
  } else {
    if (posledniSmer != smer) {
      odrazRychlost = -smer * (odrazSil + 3.5f);
      odrazPozice = hodnota;
      posledniSmer = smer;
    }
    odrazRychlost += (smer * odrazSil - odrazRychlost) * 0.18f;
  }
  odrazPozice += odrazRychlost;
  if (odrazPozice < 0) {
    odrazPozice = 0;
    odrazRychlost *= -0.4f;
  }
  if (odrazPozice > 63) {
    odrazPozice = 63;
    odrazRychlost *= -0.4f;
  }
  int odrazIndex = (int)round(odrazPozice);

  targetPos += (targetMovingRight ? 1.0f : -1.0f) * (0.5f + level * 0.18f);
  if (targetPos <= 0) {
    targetPos = 0;
    targetMovingRight = true;
  }
  if (targetPos >= 64 - targetSize) {
    targetPos = 64 - targetSize;
    targetMovingRight = false;
  }
  randNumber = (int)targetPos;

  // 4. Vykresleni fyzickeho LED pasku
  for (int i = 0; i < 64; i++) {
    if (i == hodnota) {
      np.neoPixelSetValue((uint8_t)i, 120, 0, 0, true);   // Cervena kulicka
    } else if (i == odrazIndex && abs(i - hodnota) > 1) {
      np.neoPixelSetValue((uint8_t)i, 0, 220, 255, true); // Modro-zeleny odraz
    } else if (i >= randNumber && i < randNumber + targetSize) {
      np.neoPixelSetValue((uint8_t)i, 20, 20, 0, true);   // Zluty cil
    } else {
      np.neoPixelSetValue((uint8_t)i, 0, 0, 10, true);    // Modre pozadi
    }
  }

  // 5. Herni logika
  if (hodnota >= randNumber && hodnota < randNumber + targetSize) {
    citac++;
  } else {
    citac = 0;
  }

  if (citac > 25) {
    score += 10 + level * 5;
    if (score > bestScore) bestScore = score;
    level++;
    targetSize = max(6, 10 - (level / 3));
    targetPos = random(0, 64 - targetSize);
    targetMovingRight = random(0, 2) == 0;
    randNumber = (int)targetPos;
    citac = 0;
  }

  WiFiClient client = server.available();
  if (client) {
    String currentLine = "";
    bool isDataRequest = false;

    while (client.connected()) {
      if (client.available()) {
        char c = client.read();

        if (c == '\n') {
          if (currentLine.length() == 0) {
            if (isDataRequest) {
              client.println("HTTP/1.1 200 OK");
              client.println("Content-Type: text/plain");
              client.println("Access-Control-Allow-Origin: *");
              client.println("Connection: close\r\n");
              
              client.print(x, 2); client.print(",");
              client.print(y, 2); client.print(",");
              client.print(z, 2); client.print(",");
              client.print(hodnota); client.print(",");
              client.print(randNumber); client.print(",");
              client.print(rychlost, 2); client.print(",");
              client.print(smer); client.print(",");
              client.print(odrazIndex); client.print(",");
              client.print(level); client.print(",");
              client.print(score); client.print(",");
              client.println(bestScore);
            } else {
              client.println("HTTP/1.1 200 OK");
              client.println("Content-Type: text/html; charset=UTF-8");
              client.println("Connection: close\r\n");

              client.println(F("<!DOCTYPE html><html><head><meta charset='UTF-8'><title>Rehabko Monitor</title>"));
              client.println(F("<style>"));
              client.println(F("body{font-family:Segoe UI,sans-serif;background:#181818;color:#eee;text-align:center;padding:20px;}"));
              client.println(F(".track{width:640px;height:46px;background:#0d1117;margin:35px auto;border-radius:23px;position:relative;border:2px solid #30363d;box-shadow:inset 0 2px 6px #000;}"));
              client.println(F("#target{position:absolute;height:100%;background:#e3b341;opacity:0.65;border-radius:8px;}"));
              client.println(F("#reflection{position:absolute;width:24px;height:24px;background:#73e4ff;border-radius:50%;top:11px;box-shadow:0 0 12px #73e4ff;opacity:0.9;transition:left 0.04s ease-out;}"));
              client.println(F("#ball{position:absolute;width:34px;height:34px;background:#f85149;border-radius:50%;top:6px;box-shadow:0 0 12px #f3534a;transition:left 0.04s ease-out;}"));
              client.println(F(".card{background:#21262d;display:inline-block;padding:15px 30px;border-radius:10px;margin:10px;border:1px solid #30363d;}"));
              client.println(F("h1{margin-bottom:5px;} b{color:#58a6ff;font-size:1.3em;}"));
              client.println(F("</style></head><body>"));

              client.println(F("<h1>Rehabilitační pomůcka</h1><p>Živý monitor náklonu</p>"));
              client.println(F("<div class='track'><div id='target'></div><div id='reflection'></div><div id='ball'></div></div>"));
              client.println(F("<div class='card'><p>Náklon X: <br><b id='valX'>0.00</b> g</p></div>"));
              client.println(F("<div class='card'><p>Poloha diody: <br><b id='valD'>0</b> / 63</p></div>"));
              client.println(F("<div class='card'><p>Rychlost: <br><b id='valSpeed'>0.00</b> LED/s</p></div>"));
              client.println(F("<div class='card'><p>Směr: <br><b id='valDirection'>stojí</b></p></div>"));
              client.println(F("<div class='card'><p>Odraz: <br><b id='valReflection'>0</b></p></div>"));
              client.println(F("<div class='card'><p>Level: <br><b id='valLevel'>1</b></p></div>"));
              client.println(F("<div class='card'><p>Skóre: <br><b id='valScore'>0</b></p></div>"));

              client.println(F("<script>"));
              client.println(F("function loopData(){"));
              client.println(F("  fetch('/data').then(r=>r.text()).then(txt=>{"));
              client.println(F("    let d = txt.split(',');"));
              client.println(F("    document.getElementById('valX').innerText = d[0];"));
              client.println(F("    document.getElementById('valD').innerText = d[3];"));
              client.println(F("    document.getElementById('valSpeed').innerText = d[5];"));
              client.println(F("    document.getElementById('valDirection').innerText = d[6] > 0 ? 'doprava' : (d[6] < 0 ? 'doleva' : 'stojí');"));
              client.println(F("    document.getElementById('valReflection').innerText = d[7];"));
              client.println(F("    document.getElementById('valLevel').innerText = d[8];"));
              client.println(F("    document.getElementById('valScore').innerText = d[9];"));
              client.println(F("    document.getElementById('ball').style.left = (parseFloat(d[3]) * 9.5) + 'px';"));
              client.println(F("    document.getElementById('reflection').style.left = (parseFloat(d[7]) * 9.5) + 'px';"));
              client.println(F("    document.getElementById('target').style.left = (parseFloat(d[4]) * 10) + 'px';"));
              client.println(F("    document.getElementById('target').style.width = '100px';"));
              client.println(F("  }).catch(()=>{}).finally(()=>{ setTimeout(loopData, 35); });"));
              client.println(F("}"));
              client.println(F("loopData();"));
              client.println(F("</script></body></html>"));
            }
            break;
          } else {
            currentLine = "";
          }
        } else if (c != '\r') {
          currentLine += c;
          if (currentLine.endsWith("GET /data")) {
            isDataRequest = true;
          }
        }
      }
    }
    client.stop();
  }
}