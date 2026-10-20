cpp
#include <NeoPixelConnect.h>
#include <Arduino_LSM6DSOX.h>
#include <WiFiNINA.h>
#include <SPI.h>

#define POCET_LED 16
#define PIN_LED 4
#define SMER_X 1

NeoPixelConnect np(PIN_LED, POCET_LED, pio0, 0);

char ssid[] = "RP2040";
char pass[] = "ArduinoWeb";

WiFiServer server(80);

float x, y, z;
float xf = 0, yf = 0;
float bx = 0, by = 0;

int faze = 0;
int cil = 0;
int kulicka = 0;
int presnost = 0;

void cti() {
  if (IMU.accelerationAvailable()) {
    IMU.readAcceleration(x, y, z);

    // Vyhlazeni pohybu senzoru
    xf = 0.8f * xf + 0.2f * x;
    yf = 0.8f * yf + 0.2f * y;
  }
}

int mapFloat(float v, float in_min, float in_max,
             int out_min, int out_max) {
  float vysledek =
      (v - in_min) * (out_max - out_min) /
      (in_max - in_min) + out_min;

  return (int)(vysledek + 0.5f);
}

void svit(int p, int r, int g, int b) {
  if (p < 0 || p >= POCET_LED) return;

  np.neoPixelClear(false);
  np.neoPixelSetValue(p, r, g, b, false);
  np.neoPixelShow();
}

void posliStranku(WiFiClient &client) {
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html; charset=UTF-8");
  client.println("Connection: close");
  client.println();

  client.println(F("<!DOCTYPE html><html><head><meta charset='UTF-8'>"));
  client.println(F("<meta name='viewport' content='width=device-width,initial-scale=1'>"));
  client.println(F("<title>Hra na paměť</title><style>"));
  client.println(F("body{font-family:Arial,sans-serif;background:#181818;color:#eee;text-align:center;padding:20px}"));
  client.println(F(".pas{display:flex;gap:6px;max-width:640px;margin:30px auto;background:#0d1117;padding:14px;border-radius:14px;border:2px solid #30363d}"));
  client.println(F(".d{flex:1;aspect-ratio:1;border-radius:50%;background:#1f2430}"));
  client.println(F("#st{font-size:1.3em;font-weight:bold;min-height:36px}"));
  client.println(F(".card{background:#21262d;display:inline-block;padding:12px 24px;border-radius:10px;margin:8px;border:1px solid #30363d}"));
  client.println(F("#his{line-height:1.8;color:#9aa0b2}"));
  client.println(F("</style></head><body>"));

  client.println(F("<h1>Hra na paměť</h1>"));
  client.println(F("<div class='pas' id='pas'></div>"));
  client.println(F("<div id='st'>Připojuji se...</div>"));
  client.println(F("<div class='card'>Přesnost<br><b id='pr' style='font-size:1.6em'>-</b></div>"));
  client.println(F("<h3>Poslední výsledky</h3><div id='his'></div>"));

  client.println(F("<script>"));
  client.println(F("var N=16,d=[],pred=-1,his=[],posledni='';"));
  client.println(F("var pas=document.getElementById('pas');"));
  client.println(F("for(var i=0;i<N;i++){var e=document.createElement('div');e.className='d';pas.appendChild(e);d.push(e);}"));

  client.println(F("var txt=['Zapamatuj si políčko a drž tyč rovně!','Nakláněj kuličku na místo cíle','Zamykám tip, drž tyč v poloze','Výsledek'];"));

  client.println(F("function smycka(){"));
  client.println(F("fetch('/data').then(function(r){if(!r.ok)throw Error();return r.text();}).then(function(t){"));
  client.println(F("var a=t.trim().split(',');if(a.length<4)return;"));
  client.println(F("var f=Number(a[0]),k=Number(a[1]),c=Number(a[2]),p=Number(a[3]);"));
  client.println(F("if(f<0||f>3||k<0||k>=N||c<0||c>=N)return;"));

  client.println(F("for(var i=0;i<N;i++)d[i].style.background='#1f2430';"));
  client.println(F("if(f===0)d[c].style.background='#ff7800';"));
  client.println(F("if(f===1)d[k].style.background='#0014ff';"));
  client.println(F("if(f===2)d[k].style.background='#8b2cff';"));

  client.println(F("var s=txt[f];"));
  client.println(F("if(f===3){"));
  client.println(F("d[c].style.background='#00e060';"));
  client.println(F("if(k!==c)d[k].style.background='#ff1e1e';"));
  client.println(F("s=k===c?'Trefa!':'Vedle o '+Math.abs(k-c)+' políček';"));
  client.println(F("document.getElementById('pr').innerText=p+' %';"));

  client.println(F("var vys=c+','+k+','+p;"));
  client.println(F("if(vys!==posledni){his.unshift('Cíl '+(c+1)+' · tip '+(k+1)+' · '+p+' %');his=his.slice(0,5);posledni=vys;"));
  client.println(F("document.getElementById('his').innerHTML=his.join('<br>');}"));
  client.println(F("}else if(f===0){document.getElementById('pr').innerText='-';}"));

  client.println(F("document.getElementById('st').innerText=s;"));
  client.println(F("pred=f;"));
  client.println(F("}).catch(function(){document.getElementById('st').innerText='Čekám na zařízení...';}).finally(function(){setTimeout(smycka,300);});"));
  client.println(F("}"));
  client.println(F("smycka();"));
  client.println(F("</script></body></html>"));
}

void web() {
  WiFiClient client = server.available();
  if (!client) return;

  String radek = "";
  String prvni = "";
  unsigned long zacatek = millis();

  while (client.connected() && millis() - zacatek < 300) {
    while (client.available()) {
      char c = client.read();

      if (c == '\n') {
        if (radek.length() == 0) {
          if (prvni.indexOf("GET /data") >= 0) {
            client.println("HTTP/1.1 200 OK");
            client.println("Content-Type: text/plain; charset=UTF-8");
            client.println("Cache-Control: no-store");
            client.println("Connection: close");
            client.println();

            client.print(faze);
            client.print(",");
            client.print(kulicka);
            client.print(",");
            client.print(cil);
            client.print(",");
            client.println(presnost);
          } else if (prvni.indexOf("GET / ") >= 0 ||
                     prvni.indexOf("GET /?") >= 0) {
            posliStranku(client);
          } else {
            client.println("HTTP/1.1 404 Not Found");
            client.println("Connection: close");
            client.println();
          }

          client.stop();
          return;
        }

        if (prvni.length() == 0) {
          prvni = radek;
        }

        radek = "";
      } else if (c != '\r') {
        if (radek.length() < 150) {
          radek += c;
        }
      }
    }
  }

  client.stop();
}

void cekej(unsigned long ms) {
  unsigned long zacatek = millis();

  while (millis() - zacatek < ms) {
    cti();
    web();
    delay(5);
  }
}

void setup() {
  Serial.begin(115200);
  delay(1500);

  if (!IMU.begin()) {
    Serial.println("Chyba IMU!");
    while (1) {
      delay(100);
    }
  }

  int stav = WiFi.beginAP(ssid, pass);

  if (stav != WL_AP_LISTENING) {
    Serial.println("Chyba WiFi!");
    while (1) {
      delay(100);
    }
  }

  delay(2000);
  server.begin();

  Serial.println("Wi-Fi: RP2040");
  Serial.println("Heslo: ArduinoWeb");
  Serial.println("Stranka: http://192.168.4.1");

  randomSeed(analogRead(A0));

  svit(0, 0, 20, 0);
}

void loop() {
  // 1. Ukazani nahodneho cile
  cil = random(0, POCET_LED);
  faze = 0;
  presnost = 0;

  svit(cil, 120, 60, 0);
  cekej(2000);

  // Kalibrace rovne tyce
  bx = xf;
  by = yf;

  // 2. Hledani polohy
  kulicka = POCET_LED / 2;

  unsigned long drziOd = 0;
  unsigned long vypis = 0;
  bool predchoziZamknuti = false;

  faze = 1;

  while (true) {
    cti();
    web();

    float dx = (xf - bx) * SMER_X;
    float dy = yf - by;

    bool zamyka = (dy > 0.35f || dy < -0.35f);

    if (!zamyka) {
      faze = 1;

      // Omezeni rozsahu pred prevodem na LED
      if (dx < -0.5f) dx = -0.5f;
      if (dx > 0.5f) dx = 0.5f;

      kulicka = mapFloat(dx, -0.5f, 0.5f,
                         0, POCET_LED - 1);

      kulicka = constrain(kulicka, 0, POCET_LED - 1);

      drziOd = 0;
      predchoziZamknuti = false;
    } else {
      faze = 2;

      if (!predchoziZamknuti) {
        drziOd = millis();
        predchoziZamknuti = true;
      }

      if (millis() - drziOd >= 1000) {
        break;
      }
    }

    if (zamyka) {
      svit(kulicka, 80, 0, 120);
    } else {
      svit(kulicka, 0, 20, 120);
    }

    if (millis() - vypis > 300) {
      vypis = millis();

      Serial.print("dx: ");
      Serial.print(dx, 2);
      Serial.print("  dy: ");
      Serial.print(dy, 2);
      Serial.print("  kulicka: ");
      Serial.println(kulicka + 1);
    }

    delay(10);
  }

  // 3. Vyhodnoceni
  int vzdalenost = abs(kulicka - cil);

  presnost = 100 -
      vzdalenost * 100 / (POCET_LED - 1);

  presnost = constrain(presnost, 0, 100);
  faze = 3;

  Serial.print("Cil: ");
  Serial.println(cil + 1);

  Serial.print("Tip: ");
  Serial.println(kulicka + 1);

  Serial.print("Presnost: ");
  Serial.print(presnost);
  Serial.println(" %");

  np.neoPixelClear(false);
  np.neoPixelSetValue(cil, 0, 120, 0, false);

  if (kulicka != cil) {
    np.neoPixelSetValue(kulicka, 120, 0, 0, false);
  }

  np.neoPixelShow();

  cekej(4000);
}