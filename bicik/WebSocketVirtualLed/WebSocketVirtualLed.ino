#include <NeoPixelConnect.h>
#include <Arduino_LSM6DSOX.h>
#include <WiFiNINA.h>
#include <SPI.h>

#define MAXIMUM_NUM_NEOPIXELS 64
#define ZONE_SIZE 10
#define MAX_START 54

// ---------- Nastaveni hry (lze menit pres Web API /api/config) ----------
const int LEVELS = 5;
int   goalsPerLevel = 3;          // pocet cilu v levelu
unsigned long holdMs = 800;       // jak dlouho se drzet v cili
float speedMul = 1.0;             // nasobek rychlosti cile
// rychlost cile v LED/s pro level 1..5 (level 1 = cil stoji)
const float LEVEL_SPEED[LEVELS] = {0.0, 3.0, 6.0, 10.0, 15.0};

NeoPixelConnect np(4, MAXIMUM_NUM_NEOPIXELS, pio0, 0);

char ssid[] = "RP2040";
char pass[] = "ArduinoWeb";
WiFiServer server(80);

float x, y, z;

// ---------- Stav hry ----------
float targetPos;
int   targetDir = 1;
int   level = 1;
int   goalsInLevel = 0;
long  score = 0;
int   state = 0;                  // 0 = hra bezi, 1 = konec
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
  Serial.print("Level ");   Serial.print(level);
  Serial.print("/");        Serial.print(LEVELS);
  Serial.print(" | cil ");  Serial.print(goalsInLevel);
  Serial.print("/");        Serial.print(goalsPerLevel);
  Serial.print(" | skore ");Serial.print(score);
  Serial.print(" | presnost "); Serial.print(accuracy(), 0);
  Serial.print("% | cas "); Serial.print((millis() - gameStart) / 1000);
  Serial.print(" s | ");    Serial.println(hodnoceni());
}

// ---------- Web API pomocne funkce ----------
// Vytahne cislo parametru z prvni radky pozadavku, napr. "hold=800"
bool getParam(const String &line, const char *key, float &out) {
  int idx = line.indexOf(key);
  if (idx < 0) return false;
  int start = idx + strlen(key);
  int end = start;
  while (end < (int)line.length() && line[end] != '&' && line[end] != ' ') end++;
  out = line.substring(start, end).toFloat();
  return true;
}

void sendJsonHeader(WiFiClient &c) {
  c.println("HTTP/1.1 200 OK");
  c.println("Content-Type: application/json");
  c.println("Access-Control-Allow-Origin: *");
  c.println("Connection: close");
  c.println();
}

void sendStatusJson(WiFiClient &c) {
  unsigned long endT = (state == 1) ? gameEnd : millis();
  sendJsonHeader(c);
  c.print("{\"x\":");        c.print(x, 2);
  c.print(",\"ball\":");     c.print(hodnota);
  c.print(",\"target\":");   c.print(targetPos, 1);
  c.print(",\"level\":");    c.print(level);
  c.print(",\"levels\":");   c.print(LEVELS);
  c.print(",\"goal\":");     c.print(goalsInLevel);
  c.print(",\"goals\":");    c.print(goalsPerLevel);
  c.print(",\"score\":");    c.print(score);
  c.print(",\"time\":");     c.print((endT - gameStart) / 1000.0, 1);
  c.print(",\"accuracy\":"); c.print(accuracy(), 0);
  c.print(",\"state\":");    c.print(state);
  c.print(",\"message\":\""); c.print(hodnoceni()); c.println("\"}");
}

void sendConfigJson(WiFiClient &c) {
  sendJsonHeader(c);
  c.print("{\"hold\":");  c.print(holdMs);
  c.print(",\"goals\":"); c.print(goalsPerLevel);
  c.print(",\"speed\":"); c.print(speedMul, 2);
  c.print(",\"levels\":"); c.print(LEVELS);
  c.println("}");
}

void sendPage(WiFiClient &client) {
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html; charset=UTF-8");
  client.println("Connection: close");
  client.println();

  client.println(F("<!DOCTYPE html><html><head><meta charset='UTF-8'><title>Rehabko Monitor</title>"));
  client.println(F("<style>"));
  client.println(F("body{font-family:Segoe UI,sans-serif;background:#181818;color:#eee;text-align:center;padding:20px;}"));
  client.println(F(".track{width:640px;height:46px;background:#0d1117;margin:35px auto;border-radius:23px;position:relative;border:2px solid #30363d;box-shadow:inset 0 2px 6px #000;}"));
  client.println(F("#target{position:absolute;height:100%;width:100px;background:#3fb950;opacity:0.65;border-radius:8px;}"));
  client.println(F("#ball{position:absolute;width:34px;height:34px;background:#f85149;border-radius:50%;top:6px;box-shadow:0 0 12px #f85149;}"));
  client.println(F(".card{background:#21262d;display:inline-block;padding:12px 24px;border-radius:10px;margin:8px;border:1px solid #30363d;}"));
  client.println(F("h1{margin-bottom:5px;} b{color:#58a6ff;font-size:1.3em;}"));
  client.println(F("#msg{font-size:1.2em;margin:10px;color:#e3b341;min-height:1.5em;}"));
  client.println(F("input{width:70px;background:#0d1117;color:#eee;border:1px solid #30363d;border-radius:6px;padding:4px;text-align:center;}"));
  client.println(F("button{background:#3fb950;border:0;color:#000;font-size:1em;padding:8px 20px;border-radius:8px;cursor:pointer;margin:6px;}"));
  client.println(F("#over{display:none;position:fixed;top:0;left:0;width:100%;height:100%;background:rgba(0,0,0,.85);z-index:10;}"));
  client.println(F("#box{background:#21262d;border:2px solid #3fb950;border-radius:16px;width:380px;margin:12% auto;padding:30px;}"));
  client.println(F("#box b{color:#3fb950;font-size:1.8em;}"));
  client.println(F("</style></head><body>"));

  client.println(F("<h1>Rehabilitační pomůcka</h1><p>Živý monitor náklonu</p>"));
  client.println(F("<div class='track'><div id='target'></div><div id='ball'></div></div>"));
  client.println(F("<div id='msg'></div>"));
  client.println(F("<div class='card'>Level<br><b id='lv'>1</b> / <span id='lvs'>5</span></div>"));
  client.println(F("<div class='card'>Splněno<br><b id='gl'>0</b> / <span id='gls'>3</span></div>"));
  client.println(F("<div class='card'>Skóre<br><b id='sc'>0</b></div>"));
  client.println(F("<div class='card'>Čas<br><b id='tm'>0</b> s</div>"));
  client.println(F("<div class='card'>Přesnost<br><b id='ac'>0</b> %</div>"));

  client.println(F("<div class='card'><b style='font-size:1em'>Nastavení (Web API)</b><br>"));
  client.println(F("Držení v cíli [ms] <input id='cHold' type='number'> "));
  client.println(F("Cílů v levelu <input id='cGoals' type='number'> "));
  client.println(F("Rychlost × <input id='cSpeed' type='number' step='0.1'><br>"));
  client.println(F("<button onclick='saveCfg()'>Uložit</button><button onclick='restart()'>Restart hry</button> <span id='cfgmsg'></span></div>"));

  client.println(F("<div id='over'><div id='box'><h1>Konec hry!</h1>"));
  client.println(F("<p>Konečné skóre:<br><b id='fs'>0</b></p>"));
  client.println(F("<p>Celkový čas:<br><b id='ft'>0</b> s</p>"));
  client.println(F("<p>Přesnost:<br><b id='fa'>0</b> %</p>"));
  client.println(F("<button onclick='restart()'>Hrát znovu</button></div></div>"));

  client.println(F("<script>"));
  client.println(F("function $(i){return document.getElementById(i);}"));
  client.println(F("function restart(){fetch('/api/restart').then(r=>r.json()).then(()=>{$('over').style.display='none';}).catch(()=>{});}"));
  client.println(F("function loadCfg(){fetch('/api/config').then(r=>r.json()).then(c=>{$('cHold').value=c.hold;$('cGoals').value=c.goals;$('cSpeed').value=c.speed;}).catch(()=>{});}"));
  client.println(F("function saveCfg(){fetch('/api/config?hold='+$('cHold').value+'&goals='+$('cGoals').value+'&speed='+$('cSpeed').value)"));
  client.println(F("  .then(r=>r.json()).then(c=>{$('cHold').value=c.hold;$('cGoals').value=c.goals;$('cSpeed').value=c.speed;$('cfgmsg').innerText='Uloženo';setTimeout(()=>{$('cfgmsg').innerText='';},1500);}).catch(()=>{});}"));
  client.println(F("function loopData(){"));
  client.println(F("  fetch('/api/status').then(r=>r.json()).then(d=>{"));
  client.println(F("    $('ball').style.left = (d.ball * 9.5) + 'px';"));
  client.println(F("    $('target').style.left = (d.target * 10) + 'px';"));
  client.println(F("    $('lv').innerText = d.level; $('lvs').innerText = d.levels;"));
  client.println(F("    $('gl').innerText = d.goal; $('gls').innerText = d.goals;"));
  client.println(F("    $('sc').innerText = d.score; $('tm').innerText = d.time; $('ac').innerText = d.accuracy;"));
  client.println(F("    $('msg').innerText = d.accuracy>=70 ? 'Skvěle, držíš se v cíli!' : (d.accuracy>=40 ? 'Dobře, jen tak dál.' : 'Zkus se víc soustředit na zelenou zónu.');"));
  client.println(F("    if(d.state==1){ $('fs').innerText=d.score; $('ft').innerText=d.time; $('fa').innerText=d.accuracy; $('over').style.display='block'; }"));
  client.println(F("  }).catch(()=>{}).finally(()=>{ setTimeout(loopData, 40); });"));
  client.println(F("}"));
  client.println(F("loadCfg(); loopData();"));
  client.println(F("</script></body></html>"));
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
  Serial.println("API: /api/status  /api/config  /api/restart");

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
    // 2. Plynuly pohyb cile
    float speed = LEVEL_SPEED[level - 1] * speedMul;
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
      if (now - holdStart >= holdMs) {
        long secs = (now - goalSpawn) / 1000;
        long base = 100 - secs * 3;
        if (base < 20) base = 20;
        score += base * level;
        goalsInLevel++;
        Serial.print(">>> Cil splnen! Skore: ");
        Serial.println(score);

        if (goalsInLevel >= goalsPerLevel) {
          goalsInLevel = 0;
          level++;
          if (level > LEVELS) {
            level = LEVELS;
            state = 1;
            gameEnd = now;
            np.neoPixelFill(0, 60, 0, true);
            Serial.println("=========== KONEC HRY ===========");
            Serial.print("Konecne skore: "); Serial.println(score);
            Serial.print("Celkovy cas: ");   Serial.print((gameEnd - gameStart) / 1000.0, 1); Serial.println(" s");
            Serial.print("Presnost: ");      Serial.print(accuracy(), 0); Serial.println("%");
          } else {
            Serial.print("*** LEVEL "); Serial.println(level);
          }
        }
        if (state == 0) spawnTarget();
      }
    } else {
      holdStart = 0;
    }

    // 4. Vykresleni LED pasku
    for (int i = 0; i < 64; i++) {
      if (i == hodnota)                               np.neoPixelSetValue((uint8_t)i, 120, 0, 0, false);
      else if (i >= tStart && i < tStart + ZONE_SIZE) np.neoPixelSetValue((uint8_t)i, 0, 30, 0, false);
      else                                            np.neoPixelSetValue((uint8_t)i, 0, 0, 10, false);
    }
    np.neoPixelShow();

    // 5. Prubezny vypis kazdou sekundu
    if (now - lastReport >= 1000) {
      lastReport = now;
      printStatus();
    }
  }

  // 6. Web server + REST API
  WiFiClient client = server.available();
  if (client) {
    String currentLine = "";
    String reqLine = "";
    bool firstLine = true;

    while (client.connected()) {
      if (client.available()) {
        char c = client.read();

        if (c == '\n') {
          if (firstLine) {            // ulozime prvni radek pozadavku (GET ...)
            reqLine = currentLine;
            firstLine = false;
          }
          if (currentLine.length() == 0) {   // konec hlavicek -> odpoved
            if (reqLine.indexOf("GET /api/status") >= 0) {
              sendStatusJson(client);
            } else if (reqLine.indexOf("GET /api/restart") >= 0) {
              resetGame();
              sendJsonHeader(client);
              client.println("{\"ok\":true}");
            } else if (reqLine.indexOf("GET /api/config") >= 0) {
              float v;
              if (getParam(reqLine, "hold=", v))  holdMs = constrain((long)v, 300, 5000);
              if (getParam(reqLine, "goals=", v)) goalsPerLevel = constrain((int)v, 1, 10);
              if (getParam(reqLine, "speed=", v)) speedMul = constrain(v, 0.5, 3.0);
              sendConfigJson(client);
            } else {
              sendPage(client);
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