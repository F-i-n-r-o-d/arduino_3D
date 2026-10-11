#include <NeoPixelConnect.h>
#include <Arduino_LSM6DSOX.h>
#include <WiFiNINA.h>
#include <SPI.h>

#define MAXIMUM_NUM_NEOPIXELS 64

NeoPixelConnect np(4, MAXIMUM_NUM_NEOPIXELS, pio0, 0);

int randNumber;
int citac = 0;
float x, y, z;
char ssid[] = "RP2040";        // Wi-Fi SSID
char pass[] = "ArduinoWeb";    // Wi-Fi heslo

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
}

void loop() {
  // 1. Cteni akcelerometru
  if (IMU.accelerationAvailable()) {
    IMU.readAcceleration(x, y, z);
  }

  int hodnota = mapFloat(x, -0.5, 0.5, 0, 63);
  if (hodnota > 63) hodnota = 63;
  if (hodnota < 0) hodnota = 0;

  // 2. Vykresleni fyzickeho LED pasku
  for (int i = 0; i < 64; i++) {
    if (i == hodnota) {
      np.neoPixelSetValue((uint8_t)i, 120, 0, 0, true);   // Cervena kulicka
    } else if (i >= randNumber && i < randNumber + 10) {
      np.neoPixelSetValue((uint8_t)i, 20, 20, 0, true);   // Zluty cil
    } else {
      np.neoPixelSetValue((uint8_t)i, 0, 0, 10, true);    // Modre pozadi
    }
  }

  // 3. Herni logika
  if (hodnota >= randNumber && hodnota < randNumber + 10) citac++;
  if (citac > 25) {
    randNumber = random(0, 54);
    citac = 0;
  }

  // 4. Obsluha Web Serveru
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
              // Rychla odpoved s telemetrii
              client.println("HTTP/1.1 200 OK");
              client.println("Content-Type: text/plain");
              client.println("Access-Control-Allow-Origin: *");
              client.println("Connection: close\r\n");
              
              client.print(x, 2); client.print(",");
              client.print(y, 2); client.print(",");
              client.print(z, 2); client.print(",");
              client.print(hodnota); client.print(",");
              client.println(randNumber);
            } else {
              // Kompletni HTML stranka s grafickym rozhranim
              client.println("HTTP/1.1 200 OK");
              client.println("Content-Type: text/html; charset=UTF-8");
              client.println("Connection: close\r\n");

              client.println(F("<!DOCTYPE html><html><head><meta charset='UTF-8'><title>Rehabko Monitor</title>"));
              client.println(F("<style>"));
              client.println(F("body{font-family:Segoe UI,sans-serif;background:#181818;color:#eee;text-align:center;padding:20px;}"));
              client.println(F(".track{width:640px;height:46px;background:#0d1117;margin:35px auto;border-radius:23px;position:relative;border:2px solid #30363d;box-shadow:inset 0 2px 6px #000;}"));
              client.println(F("#target{position:absolute;height:100%;background:#e3b341;opacity:0.65;border-radius:8px;}"));
              client.println(F("#ball{position:absolute;width:34px;height:34px;background:#f85149;border-radius:50%;top:6px;box-shadow:0 0 12px #f85149;transition:left 0.04s ease-out;}"));
              client.println(F(".card{background:#21262d;display:inline-block;padding:15px 30px;border-radius:10px;margin:10px;border:1px solid #30363d;}"));
              client.println(F("h1{margin-bottom:5px;} b{color:#58a6ff;font-size:1.3em;}"));
              client.println(F("</style></head><body>"));

              client.println(F("<h1>Rehabilitační pomůcka</h1><p>Živý monitor náklonu</p>"));
              client.println(F("<div class='track'><div id='target'></div><div id='ball'></div></div>"));
              client.println(F("<div class='card'><p>Náklon X: <br><b id='valX'>0.00</b> g</p></div>"));
              client.println(F("<div class='card'><p>Poloha diody: <br><b id='valD'>0</b> / 63</p></div>"));

              client.println(F("<script>"));
              client.println(F("function loopData(){"));
              client.println(F("  fetch('/data').then(r=>r.text()).then(txt=>{"));
              client.println(F("    let d = txt.split(',');"));
              client.println(F("    document.getElementById('valX').innerText = d[0];"));
              client.println(F("    document.getElementById('valD').innerText = d[3];"));
              client.println(F("    document.getElementById('ball').style.left = (parseFloat(d[3]) * 9.5) + 'px';"));
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