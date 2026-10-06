#include <NeoPixelConnect.h>
#include <Arduino_LSM6DSOX.h>
#include <WiFiNINA.h>
#include <SPI.h>

#define MAXIMUM_NUM_NEOPIXELS 64
#define ZONE_SIZE 10          // sirka cile v LED
#define MAX_START 54          // nejvyssi startovni pozice cile (64 - 10)

// ---------- Nastaveni hry ----------
const int   LEVELS          = 5;      // pocet levelu
const int   GOALS_PER_LEVEL = 3;      // kolik cilu je treba splnit v levelu
const unsigned long HOLD_MS = 800;    // jak dlouho se musi kulicka drzet v cili
// rychlost pohybu cile v LED/s pro level 1..5 (level 1 = cil stoji)
const float LEVEL_SPEED[LEVELS] = {0.0, 3.0, 6.0, 10.0, 15.0};

NeoPixelConnect np(4, MAXIMUM_NUM_NEOPIXELS, pio0, 0);

char ssid[] = "RP2040";
char pass[] = "ArduinoWeb";
WiFiServer server(80);

float x, y, z;

// ---------- Stav hry ----------
float targetPos;              // plynula pozice cile (0..54)
int   targetDir = 1;          // smer pohybu cile
int   level = 1;
int   goalsInLevel = 0;
int   goalsTotalDone = 0;
long  score = 0;
int   state = 0;              // 0 = hra bezi, 1 = konec hry
unsigned long gameStart = 0, gameEnd = 0;
unsigned long lastTick = 0, holdStart = 0, goalSpawn = 0, lastReport = 0;
unsigned long onTargetMs = 0, playedMs = 0;
int   hodnota = 0;

int mapFloat(float val, float in_min, float in_max, int out_min, int out_max) {
  return (val - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

float accuracy() {
  if (playedMs == 0) return 0;
  return 100.0 * onTargetMs / playedMs;
}

const char* hodnoceni() {
  float a = accuracy();
  if (a >= 70) return "Skvele, drzis se v cili!";
  if (a >= 40) return "Dobre, jen tak dal.";
  return "Zkus se vic soustredit na zelenou zonu.";
}

void spawnTarget() {
  targetPos = random(0, MAX_START + 1);
  targetDir = random(0, 2) ? 1 : -1;
  holdStart = 0;
  goalSpawn = millis();
}

void resetGame() {
  level = 1;
  goalsInLevel = 0;
  goalsTotalDone = 0;
  score = 0;
  state = 0;
  onTargetMs = 0;
  playedMs = 0;
  gameStart = millis();
  lastTick = gameStart;
  lastReport = gameStart;
  spawnTarget();
  Serial.println("=== NOVA HRA ===");
}

void printStatus() {
  Serial.print("Level ");
  Serial.print(level);
  Serial.print("/");
  Serial.print(LEVELS);
  Serial.print(" | cil ");
  Serial.print(goalsInLevel);
  Serial.print("/");
  Serial.print(GOALS_PER_LEVEL);
  Serial.print(" | skore ");
  Serial.print(score);
  Serial.print(" | presnost ");
  Serial.print(accuracy(), 0);
  Serial.print("% | cas ");
  Serial.print((millis() - gameStart) / 1000);
  Serial.print(" s | ");
  Serial.println(hodnoceni());
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

  randomSeed(analogRead(A0) ^ micros());
  resetGame();
}

void loop() {
  unsigned long now = millis();
  unsigned long dtMs = now - lastTick;
  lastTick = now;

  // 1. Cteni akcelerometru
  if (IMU.accelerationAvailable()) {
    IMU.readAcceleration(x, y, z);
  }
  hodnota = mapFloat(x, -0.5, 0.5, 0, 63);
  if (hodnota > 63) hodnota = 63;
  if (hodnota < 0) hodnota = 0;

  if (state == 0) {
    // 2. Pohyb ciloveho pole (plynuly, odrazi se od kraju, s levelem zrychluje)
    float speed = LEVEL_SPEED[level - 1];
    if (speed > 0) {
      targetPos += targetDir * speed * (dtMs / 1000.0);
      if (targetPos >= MAX_START) { targetPos = MAX_START; targetDir = -1; }
      if (targetPos <= 0)         { targetPos = 0;         targetDir = 1;  }
    }
    int tStart = (int)targetPos;

    // 3. Herni logika
    bool inTarget = (hodnota >= tStart && hodnota < tStart + ZONE_SIZE);
    playedMs += dtMs;
    if (inTarget) {
      onTargetMs += dtMs;
      if (holdStart == 0) holdStart = now;
      if (now - holdStart >= HOLD_MS) {
        // cil splnen - body: rychlejsi splneni = vic bodu, vyssi level = nasobek
        long secs = (now - goalSpawn) / 1000;
        long base = 100 - secs * 3;
        if (base < 20) base = 20;
        score += base * level;
        goalsInLevel++;
        goalsTotalDone++;
        Serial.print(">>> Cil splnen! Skore: ");
        Serial.println(score);

        if (goalsInLevel >= GOALS_PER_LEVEL) {
          goalsInLevel = 0;
          level++;
          if (level > LEVELS) {
            level = LEVELS;
            state = 1;
            gameEnd = now;
            np.neoPixelFill(0, 60, 0, true);
            Serial.println("=========== KONEC HRY ===========");
            Serial.print("Konecne skore: ");
            Serial.println(score);
            Serial.print("Celkovy cas: ");
            Serial.print((gameEnd - gameStart) / 1000.0, 1);
            Serial.println(" s");
            Serial.print("Presnost: ");
            Serial.print(accuracy(), 0);
            Serial.println("%");
          } else {
            Serial.print("*** LEVEL ");
            Serial.print(level);
            Serial.print(" - cil se pohybuje rychlosti ");
            Serial.print(LEVEL_SPEED[level - 1], 0);
            Serial.println(" LED/s");
          }
        }
        if (state == 0) spawnTarget();
      }
    } else {
      holdStart = 0;
    }

    // 4. Vykresleni fyzickeho LED pasku
    for (int i = 0; i < 64; i++) {
      if (i == hodnota)                          np.neoPixelSetValue((uint8_t)i, 120, 0, 0, false);  // kulicka
      else if (i >= tStart && i < tStart + ZONE_SIZE) np.neoPixelSetValue((uint8_t)i, 0, 30, 0, false);   // cil
      else                                       np.neoPixelSetValue((uint8_t)i, 0, 0, 10, false);   // pozadi
    }
    np.neoPixelShow();

    // 5. Prubezny vypis uzivateli kazdou sekundu
    if (now - lastReport >= 1000) {
      lastReport = now;
      printStatus();
    }
  }

  // 6. Web server
  WiFiClient client = server.available();
  if (client) {
    String currentLine = "";
    bool isDataRequest = false;
    bool isRestart = false;

    while (client.connected()) {
      if (client.available()) {
        char c = client.read();

        if (c == '\n') {
          if (currentLine.length() == 0) {
            if (isRestart) {
              resetGame();
              client.println("HTTP/1.1 200 OK");
              client.println("Content-Type: text/plain");
              client.println("Access-Control-Allow-Origin: *");
              client.println("Connection: close\r\n");
              client.println("OK");
            } else if (isDataRequest) {
              unsigned long endT = (state == 1) ? gameEnd : millis();
              client.println("HTTP/1.1 200 OK");
              client.println("Content-Type: text/plain");
              client.println("Access-Control-Allow-Origin: *");
              client.println("Connection: close\r\n");

              // x,poziceKulicky,poziceCile,level,splnenoVLevelu,skore,casSek,presnost,stav
              client.print(x, 2);                      client.print(",");
              client.print(hodnota);                   client.print(",");
              client.print(targetPos, 1);              client.print(",");
              client.print(level);                     client.print(",");
              client.print(goalsInLevel);              client.print(",");
              client.print(score);                     client.print(",");
              client.print((endT - gameStart) / 1000.0, 1); client.print(",");
              client.print(accuracy(), 0);             client.print(",");
              client.println(state);
            } else {
              client.println("HTTP/1.1 200 OK");
              client.println("Content-Type: text/html; charset=UTF-8");
              client.println("Connection: close\r\n");

              client.println(F("<!DOCTYPE html><html><head><meta charset='UTF-8'><title>Rehabko Monitor</title>"));
              client.println(F("<style>"));
              client.println(F("body{font-family:Segoe UI,sans-serif;background:#181818;color:#eee;text-align:center;padding:20px;}"));
              client.println(F(".track{width:640px;height:46px;background:#0d1117;margin:35px auto;border-radius:23px;position:relative;border:2px solid #30363d;box-shadow:inset 0 2px 6px #000;}"));
              client.println(F("#target{position:absolute;height:100%;width:100px;background:#3fb950;opacity:0.65;border-radius:8px;}"));
              client.println(F("#ball{position:absolute;width:34px;height:34px;background:#f85149;border-radius:50%;top:6px;box-shadow:0 0 12px #f85149;}"));
              client.println(F(".card{background:#21262d;display:inline-block;padding:12px 24px;border-radius:10px;margin:8px;border:1px solid #30363d;}"));
              client.println(F("h1{margin-bottom:5px;} b{color:#58a6ff;font-size:1.3em;}"));
              client.println(F("#msg{font-size:1.2em;margin:10px;color:#e3b341;min-height:1.5em;}"));
              client.println(F("#over{display:none;position:fixed;top:0;left:0;width:100%;height:100%;background:rgba(0,0,0,.85);z-index:10;}"));
              client.println(F("#box{background:#21262d;border:2px solid #3fb950;border-radius:16px;width:380px;margin:12% auto;padding:30px;}"));
              client.println(F("#box b{color:#3fb950;font-size:1.8em;} button{background:#3fb950;border:0;color:#000;font-size:1.1em;padding:10px 24px;border-radius:8px;cursor:pointer;margin-top:15px;}"));
              client.println(F("</style></head><body>"));

              client.println(F("<h1>Rehabilitační pomůcka</h1><p>Živý monitor náklonu</p>"));
              client.println(F("<div class='track'><div id='target'></div><div id='ball'></div></div>"));
              client.println(F("<div id='msg'></div>"));
              client.println(F("<div class='card'>Level<br><b id='lv'>1</b> / 5</div>"));
              client.println(F("<div class='card'>Splněno<br><b id='gl'>0</b> / 3</div>"));
              client.println(F("<div class='card'>Skóre<br><b id='sc'>0</b></div>"));
              client.println(F("<div class='card'>Čas<br><b id='tm'>0</b> s</div>"));
              client.println(F("<div class='card'>Přesnost<br><b id='ac'>0</b> %</div>"));

              client.println(F("<div id='over'><div id='box'><h1>Konec hry!</h1>"));
              client.println(F("<p>Konečné skóre:<br><b id='fs'>0</b></p>"));
              client.println(F("<p>Celkový čas:<br><b id='ft'>0</b> s</p>"));
              client.println(F("<p>Přesnost:<br><b id='fa'>0</b> %</p>"));
              client.println(F("<button onclick='restart()'>Hrát znovu</button></div></div>"));

              client.println(F("<script>"));
              client.println(F("function $(i){return document.getElementById(i);}"));
              client.println(F("function restart(){fetch('/restart').then(()=>{$('over').style.display='none';}).catch(()=>{});}"));
              client.println(F("function loopData(){"));
              client.println(F("  fetch('/data').then(r=>r.text()).then(txt=>{"));
              client.println(F("    let d = txt.trim().split(',');"));
              client.println(F("    $('ball').style.left = (parseFloat(d[1]) * 9.5) + 'px';"));
              client.println(F("    $('target').style.left = (parseFloat(d[2]) * 10) + 'px';"));
              client.println(F("    $('lv').innerText = d[3]; $('gl').innerText = d[4];"));
              client.println(F("    $('sc').innerText = d[5]; $('tm').innerText = d[6]; $('ac').innerText = d[7];"));
              client.println(F("    let a = parseFloat(d[7]);"));
              client.println(F("    $('msg').innerText = a>=70 ? 'Skvěle, držíš se v cíli!' : (a>=40 ? 'Dobře, jen tak dál.' : 'Zkus se víc soustředit na zelenou zónu.');"));
              client.println(F("    if(d[8].trim()=='1'){ $('fs').innerText=d[5]; $('ft').innerText=d[6]; $('fa').innerText=d[7]; $('over').style.display='block'; }"));
              client.println(F("  }).catch(()=>{}).finally(()=>{ setTimeout(loopData, 40); });"));
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
          if (currentLine.endsWith("GET /data"))    isDataRequest = true;
          if (currentLine.endsWith("GET /restart")) isRestart = true;
        }
      }
    }
    client.stop();
  }
}