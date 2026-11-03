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

// Nové proměnné pro odměny a skóre
int skore = 0;
int casDrzeniSekundy = 0;
String aktualniOdmena = "Zatím žádná";

void cti() {
  if (IMU.accelerationAvailable()) {
    IMU.readAcceleration(x, y, z);
    xf = 0.8f * xf + 0.2f * x;
    yf = 0.8f * yf + 0.2f * y;
  }
}

int mapFloat(float v, float in_min, float in_max, int out_min, int out_max) {
  float vysledek = (v - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
  return (int)(vysledek + 0.5f);
}

void svit(int p, int r, int g, int b) {
  if (p < 0 || p >= POCET_LED) return;
  np.neoPixelClear(false);
  np.neoPixelSetValue(p, r, g, b, false);
  np.neoPixelShow();
}

// Opravená a vylepšená webová stránka zobrazující skóre a odměny na PC
void posliStranku(WiFiClient &client) {
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html; charset=UTF-8");
  client.println("Connection: close");
  client.println();

  client.println(F("<!DOCTYPE html><html><head><meta charset='UTF-8'>"));
  client.println(F("<meta name='viewport' content='width=device-width,initial-scale=1'>"));
  client.println(F("<title>Balancování s odměnami</title><style>"));
  client.println(F("body{font-family:Arial,sans-serif;background:#121212;color:#eee;text-align:center;padding:20px}"));
  client.println(F(".pas{display:flex;gap:6px;max-width:640px;margin:20px auto;background:#1e1e1e;padding:14px;border-radius:14px;border:2px solid #333}"));
  client.println(F(".d{flex:1;aspect-ratio:1;border-radius:50%;background:#2a2f3d}"));
  client.println(F("#st{font-size:1.4em;font-weight:bold;min-height:36px;color:#ffa116}"));
  client.println(F(".card-container{display:flex;justify-content:center;gap:15px;flex-wrap:wrap;margin:20px 0}"));
  client.println(F(".card{background:#1e1e1e;padding:15px 25px;border-radius:10px;border:1px solid #333;min-width:140px}"));
  client.println(F(".badge{color:#00e060;font-weight:bold}"));
  client.println(F("#his{line-height:1.8;color:#aaa;max-width:500px;margin:0 auto;text-align:left;background:#181818;padding:15px;border-radius:8px}"));
  client.println(F("</style></head><body>"));

  client.println(F("<h1>🎮 Balancuj a sbírej odměny</h1>"));
  client.println(F("<div class='pas' id='pas'></div>"));
  client.println(F("<div id='st'>Připojuji se...</div>"));
  
  client.println(F("<div class='card-container'>"));
  client.println(F("<div class='card'>Skóre<br><b id='sk' style='font-size:1.8em; color:#ff7800'>0</b></div>"));
  client.println(F("<div class='card'>Výdrž (čas)<br><b id='tm' style='font-size:1.8em'>0 s</b></div>"));
  client.println(F("<div class='card'>Získaná odměna<br><span id='od' class='badge'>-</span></div>"));
  client.println(F("</div>"));

  client.println(F("<h3>🏆 Síň slávy (Poslední pokusy)</h3><div id='his'>Zatím žádné hry...</div>"));

  client.println(F("<script>"));
  client.println(F("var N=16,d=[],his=[],posledni='';"));
  client.println(F("var pas=document.getElementById('pas');"));
  client.println(F("for(var i=0;i<N;i++){var e=document.createElement('div');e.className='d';pas.appendChild(e);d.push(e);}"));
  
  client.println(F("var txt=['1. Zapamatuj si cíl...','2. Nakláněj tyč na cíl!','3. Držíš pozici! Sbíráš body...','4. Výsledek kola'];"));

  client.println(F("function smycka(){"));
  client.println(F("fetch('/data').then(function(r){if(!r.ok)throw Error();return r.text();}).then(function(t){"));
  client.println(F("var a=t.trim().split(',');if(a.length<7)return;"));
  client.println(F("var f=Number(a[0]),k=Number(a[1]),c=Number(a[2]),p=Number(a[3]),s=Number(a[4]),tm=Number(a[5]),od=a[6];"));
  
  client.println(F("for(var i=0;i<N;i++)d[i].style.background='#2a2f3d';"));
  client.println(F("if(f===0)d[c].style.background='#ff7800';"));
  client.println(F("if(f===1||f===2)d[k].style.background='#007acc';"));
  
  client.println(F("document.getElementById('st').innerText=txt[f];"));
  client.println(F("document.getElementById('sk').innerText=s;"));
  client.println(F("document.getElementById('tm').innerText=tm+' s';"));
  client.println(F("document.getElementById('od').innerText=decodeURIComponent(od);"));

  client.println(F("if(f===3){"));
  client.println(F("d[c].style.background='#00e060';"));
  client.println(F("if(k!==c)d[k].style.background='#ff3333';"));
  
  client.println(F("var vys=c+','+k+','+s;"));
  client.println(F("if(vys!==posledni){his.unshift('Skóre: <b>'+s+'</b> | Výdrž: '+tm+'s | Odměna: <span style=\"color:#00e060\">'+decodeURIComponent(od)+'</span>');his=his.slice(0,5);posledni=vys;"));
  client.println(F("document.getElementById('his').innerHTML=his.join('<br>');}"));
  client.println(F("}"));

  client.println(F("}).catch(function(){}).finally(function(){setTimeout(smycka,300);});"));
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

            client.print(faze); client.print(",");
            client.print(kulicka); client.print(",");
            client.print(cil); client.print(",");
            client.print(presnost); client.print(",");
            client.print(skore); client.print(",");
            client.print(casDrzeniSekundy); client.print(",");
            client.println(aktualniOdmena);
          } else if (prvni.indexOf("GET / ") >= 0 || prvni.indexOf("GET /?") >= 0) {
            posliStranku(client);
          } else {
            client.println("HTTP/1.1 404 Not Found");
            client.println("Connection: close");
            client.println();
          }
          client.stop();
          return;
        }
        if (prvni.length() == 0) prvni = radek;
        radek = "";
      } else if (c != '\r') {
        if (radek.length() < 150) radek += c;
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
    while (1) delay(100);
  }

  WiFi.beginAP(ssid, pass);
  delay(2000);
  server.begin();

  randomSeed(analogRead(A0));
  svit(0, 0, 20, 0);
}

void loop() {
  // 1. Ukázání cíle
  cil = random(0, POCET_LED);
  faze = 0;
  skore = 0;
  casDrzeniSekundy = 0;
  aktualniOdmena = "Zatím žádná";

  svit(cil, 120, 60, 0);
  cekej(2000);

  bx = xf;
  by = yf;
  kulicka = POCET_LED / 2;

  unsigned long drziOd = 0;
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
      if (dx < -0.5f) dx = -0.5f;
      if (dx > 0.5f) dx = 0.5f;
      kulicka = mapFloat(dx, -0.5f, 0.5f, 0, POCET_LED - 1);
      kulicka = constrain(kulicka, 0, POCET_LED - 1);

      predchoziZamknuti = false;
      drziOd = 0;
      casDrzeniSekundy = 0;
    } else {
      faze = 2; // Hráč drží pozici (zamyká)
      if (!predchoziZamknuti) {
        drziOd = millis();
        predchoziZamknuti = true;
      }

      // Výpočet času výdrže v sekundách
      casDrzeniSekundy = (millis() - drziOd) / 1000;

      // MOTIVAČNÍ SYSTÉM ODMĚN A BODŮ ZA VÝDRŽ
      if (casDrzeniSekundy < 2) {
        skore = 50;
        aktualniOdmena = "Stabilizator";
      } else if (casDrzeniSekundy < 4) {
        skore = 150;
        aktualniOdmena = "Bronzova tyc";
      } else if (casDrzeniSekundy < 7) {
        skore = 300;
        aktualniOdmena = "Stribrna tyc";
      } else {
        skore = 600;
        aktualniOdmena = "Zlata tyc (Mistr!)";
      }

      // Pokud drží déle než 8 sekund, automaticky ukončíme kolo s maximální odměnou
      if (casDrzeniSekundy >= 8) {
        break;
      }
    }

    if (zamyka) {
      svit(kulicka, 180, 0, 180); // Fialová při držení
    } else {
      svit(kulicka, 0, 100, 255); // Modrá při hledání
    }

    delay(10);
  }

  // 3. Vyhodnocení kola
  int vzdalenost = abs(kulicka - cil);
  presnost = 100 - vzdalenost * 100 / (POCET_LED - 1);
  presnost = constrain(presnost, 0, 100);
  
  // Přičtení bonusu za přesnost k celkovému skóre
  skore += presnost * 2;
  faze = 3;

  // LED vizualizace výsledku
  np.neoPixelClear(false);
  np.neoPixelSetValue(cil, 0, 180, 0, false); // Zelená = cíl
  if (kulicka != cil) {
    np.neoPixelSetValue(kulicka, 180, 0, 0, false); // Červená = tvojí pozice
  }
  np.neoPixelShow();

  cekej(4000); // Zobrazení výsledků na tyči i na PC
}