#include <NeoPixelConnect.h>
#include <Arduino_LSM6DSOX.h>
#include <WiFiNINA.h>
#include <SPI.h>

#define POCET_LED 16
#define SMER_X 1          // kdyz kulicka jede na opacnou stranu, dej -1

NeoPixelConnect np(4, POCET_LED, pio0, 0);

char ssid[] = "RP2040";
char pass[] = "ArduinoWeb";
WiFiServer server(80);

float x, y, z;
float xf = 0, yf = 0;     // vyhlazene hodnoty
float bx = 0, by = 0;     // poloha "tyc rovne"

int faze = 0;             // 0 cil, 1 hleda, 2 zamyka, 3 vysledek
int cil = 0;
int kulicka = 0;
int presnost = 0;

void cti() {
  if (IMU.accelerationAvailable()) {
    IMU.readAcceleration(x, y, z);
    xf = 0.8 * xf + 0.2 * x;
    yf = 0.8 * yf + 0.2 * y;
  }
}

int mapFloat(float v, float in_min, float in_max, int out_min, int out_max) {
  return (v - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

void svit(int p, int r, int g, int b) {
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
  client.println(F("body{font-family:Segoe UI,sans-serif;background:#181818;color:#eee;text-align:center;padding:20px}"));
  client.println(F(".pas{display:flex;gap:6px;max-width:640px;margin:30px auto;background:#0d1117;padding:14px;border-radius:14px;border:2px solid #30363d}"));
  client.println(F(".d{flex:1;aspect-ratio:1;border-radius:50%;background:#1f2430}"));
  client.println(F("#st{font-size:1.4em;font-weight:600;min-height:36px}"));
  client.println(F(".card{background:#21262d;display:inline-block;padding:12px 24px;border-radius:10px;margin:8px;border:1px solid #30363d}"));
  client.println(F("#his{line-height:1.8;color:#9aa0b2}"));
  client.println(F("</style></head><body>"));
  client.println(F("<h1>Hra na paměť</h1>"));
  client.println(F("<div class='pas' id='pas'></div>"));
  client.println(F("<div id='st'></div>"));
  client.println(F("<div class='card'>Přesnost<br><b id='pr' style='font-size:1.6em'>-</b></div>"));
  client.println(F("<div id='his'></div>"));
  client.println(F("<script>"));
  client.println(F("var N=16,d=[],pred=-1,his=[];"));
  client.println(F("var pas=document.getElementById('pas');"));
  client.println(F("for(var i=0;i<N;i++){var e=document.createElement('div');e.className='d';pas.appendChild(e);d.push(e);}"));
  client.println(F("var txt=['Zapamatuj si políčko a drž tyč rovně!','Naklápěj kuličku na místo cíle','Zamykám tip, drž tyč u sebe','Výsledek'];"));
  client.println(F("function smycka(){fetch('/data').then(function(r){return r.text();}).then(function(t){"));
  client.println(F("var a=t.split(','),f=+a[0],k=+a[1],c=+a[2],p=+a[3];"));
  client.println(F("for(var i=0;i<N;i++)d[i].style.background='#1f2430';"));
  client.println(F("if(f==0)d[c].style.background='#ff7800';"));
  client.println(F("if(f==1)d[k].style.background='#0014ff';"));
  client.println(F("if(f==2)d[k].style.background='#8b2cff';"));
  client.println(F("if(f==3){d[c].style.background='#00e060';if(k!=c)d[k].style.background='#ff1e1e';}"));
  client.println(F("var s=txt[f];"));
  client.println(F("if(f==3){var v=Math.abs(k-c);s=v==0?'Trefa!':'Vedle o '+v;"));
  client.println(F("document.getElementById('pr').innerText=p+' %';"));
  client.println(F("if(pred!=3){his.unshift('Cíl '+(c+1)+' · tip '+(k+1)+' · '+p+' %');his=his.slice(0,5);"));
  client.println(F("document.getElementById('his').innerHTML=his.join('<br>');}}"));
  client.println(F("document.getElementById('st').innerText=s;pred=f;"));
  client.println(F("}).catch(function(){}).finally(function(){setTimeout(smycka,300);});}"));
  client.println(F("smycka();"));
  client.println(F("</script></body></html>"));
}

void web() {
  WiFiClient client = server.available();
  if (!client) return;

  String radek = "";
  String prvni = "";
  unsigned long t = millis();

  while (client.connected() && millis() - t < 150) {
    if (client.available()) {
      char c = client.read();
      if (c == '\n') {
        if (radek.length() == 0) {
          if (prvni.indexOf("/data") >= 0) {
            client.println("HTTP/1.1 200 OK");
            client.println("Content-Type: text/plain");
            client.println("Access-Control-Allow-Origin: *");
            client.println("Connection: close");
            client.println();
            client.print(faze);    client.print(",");
            client.print(kulicka); client.print(",");
            client.print((faze == 1 || faze == 2) ? 0 : cil); client.print(",");
            client.println(presnost);
          } else {
            posliStranku(client);
          }
          break;
        }
        if (prvni.length() == 0) prvni = radek;
        radek = "";
      } else if (c != '\r') {
        radek += c;
      }
    }
  }
  client.stop();
}

void cekej(unsigned long ms) {
  unsigned long t = millis();
  while (millis() - t < ms) {
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
    while (1);
  }
  if (WiFi.beginAP(ssid, pass) != WL_AP_LISTENING) {
    Serial.println("Chyba WiFi!");
    while (1);
  }
  delay(2000);
  server.begin();
  Serial.println("Stranka: http://192.168.4.1");
  randomSeed(analogRead(A0));
  svit(0, 0, 20, 0);
}

void loop() {
  // 1) ukaz cil a mezitim se tyc vynuluje (drz ji rovne)
  cil = random(0, POCET_LED);
  faze = 0;
  svit(cil, 120, 60, 0);
  cekej(2000);
  bx = xf;
  by = yf;

  // 2) hledani a zamknuti naklonem k sobe
  kulicka = POCET_LED / 2;
  unsigned long drziOd = 0;
  unsigned long vypis = 0;
  faze = 1;

  while (true) {
    cti();
    web();

    float dx = (xf - bx) * SMER_X;
    float dy = yf - by;
    bool zamyka = (dy > 0.35 || dy < -0.35);

    if (!zamyka) {
      faze = 1;
      kulicka = mapFloat(dx, -0.5, 0.5, 0, POCET_LED - 1);
      if (kulicka > POCET_LED - 1) kulicka = POCET_LED - 1;
      if (kulicka < 0) kulicka = 0;
      drziOd = 0;
    } else {
      faze = 2;
      if (drziOd == 0) drziOd = millis();
      if (millis() - drziOd >= 1000) break;
    }

    if (zamyka) svit(kulicka, 80, 0, 120);
    else        svit(kulicka, 0, 20, 120);

    if (millis() - vypis > 300) {
      vypis = millis();
      Serial.print("dx: "); Serial.print(dx, 2);
      Serial.print("  dy: "); Serial.print(dy, 2);
      Serial.print("  kulicka: "); Serial.println(kulicka);
    }
    delay(10);
  }

  // 3) vysledek
  int vzdalenost = abs(kulicka - cil);
  presnost = 100 - vzdalenost * 100 / (POCET_LED - 1);
  faze = 3;

  Serial.print("Cil: ");      Serial.println(cil + 1);
  Serial.print("Tip: ");      Serial.println(kulicka + 1);
  Serial.print("Presnost: "); Serial.print(presnost); Serial.println(" %");

  np.neoPixelClear(false);
  np.neoPixelSetValue(cil, 0, 120, 0, false);
  if (kulicka != cil) np.neoPixelSetValue(kulicka, 120, 0, 0, false);
  np.neoPixelShow();
  cekej(4000);
}