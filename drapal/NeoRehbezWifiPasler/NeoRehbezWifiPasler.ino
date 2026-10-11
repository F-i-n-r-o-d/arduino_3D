#include <NeoPixelConnect.h>
#include <Arduino_LSM6DSOX.h>
#include <WiFiNINA.h>
#include <SPI.h>

#define MAXIMUM_NUM_NEOPIXELS 64
NeoPixelConnect np(4, MAXIMUM_NUM_NEOPIXELS, pio0, 0);

const uint8_t LIGHT_COUNT = 5;
const uint8_t LIGHT_LEDS[LIGHT_COUNT] = {28, 30, 32, 34, 36};

char ssid[] = "RP2040";
char pass[] = "ArduinoWeb";
WiFiServer server(80);

const unsigned long LIGHT_STEP_MS = 500;
const unsigned long RANDOM_DELAY_MIN_MS = 500;
const unsigned long RANDOM_DELAY_MAX_MS = 5000;
const unsigned long RESULT_HOLD_MS = 2500;
const float JERK_THRESHOLD = 0.45f;

const uint8_t HISTORY_SIZE = 8;

enum Phase {
  PHASE_START,
  PHASE_LIGHTING,
  PHASE_WAIT_RANDOM,
  PHASE_MEASURE,
  PHASE_RESULT,
  PHASE_FALSE_START
};

Phase phase = PHASE_START;
uint8_t lightsOn = 0;
unsigned long phaseStartMs = 0;
unsigned long randomHoldMs = 0;
unsigned long reactionStartMs = 0;
unsigned long reactionTimeMs = 0;
float prevMag = 1.0f;
bool imuReady = false;

unsigned long lastValidMs = 0;
unsigned long bestMs = 0;
unsigned int validCount = 0;
unsigned int falseStartCount = 0;
unsigned long historyMs[HISTORY_SIZE];
uint8_t historyCount = 0;

const char* phaseLabel() {
  switch (phase) {
    case PHASE_START:       return "Ceka na start";
    case PHASE_LIGHTING:    return "Rozsviceni svetel";
    case PHASE_WAIT_RANDOM: return "Cekani (nahodna prodleva)";
    case PHASE_MEASURE:     return "Mereni reakce!";
    case PHASE_RESULT:      return "Vysledek";
    case PHASE_FALSE_START: return "Predcasna reakce";
    default:                return "?";
  }
}

void allLightsOff() {
  np.neoPixelClear(true);
}

void showLights(uint8_t count) {
  np.neoPixelClear(false);
  for (uint8_t i = 0; i < count && i < LIGHT_COUNT; i++) {
    np.neoPixelSetValue(LIGHT_LEDS[i], 180, 0, 0, false);
  }
  np.neoPixelShow();
}

float readAccelMagnitude() {
  float ax, ay, az;
  if (!IMU.accelerationAvailable()) {
    return prevMag;
  }
  IMU.readAcceleration(ax, ay, az);
  return sqrt(ax * ax + ay * ay + az * az);
}

bool detectJerk() {
  float mag = readAccelMagnitude();
  float delta = fabs(mag - prevMag);
  prevMag = mag;

  if (!imuReady) {
    imuReady = true;
    return false;
  }
  return delta >= JERK_THRESHOLD;
}

void recordValidResult(unsigned long ms) {
  lastValidMs = ms;

  if (bestMs == 0 || ms < bestMs) {
    bestMs = ms;
  }
  validCount++;

  if (historyCount < HISTORY_SIZE) {
    historyMs[historyCount++] = ms;
  } else {
    for (uint8_t i = 1; i < HISTORY_SIZE; i++) {
      historyMs[i - 1] = historyMs[i];
    }
    historyMs[HISTORY_SIZE - 1] = ms;
  }
}

void resetTrialToStart() {
  allLightsOff();
  lightsOn = 0;
  randomHoldMs = 0;
  reactionTimeMs = 0;
  imuReady = false;
  prevMag = readAccelMagnitude();
  phase = PHASE_START;
  phaseStartMs = millis();
  Serial.println();
  Serial.println(F(">>> Skubnutim spust dalsi pokus."));
}

void startLighting() {
  allLightsOff();
  lightsOn = 0;
  randomHoldMs = 0;
  reactionTimeMs = 0;
  imuReady = false;
  prevMag = readAccelMagnitude();
  phase = PHASE_LIGHTING;
  phaseStartMs = millis();
  Serial.println();
  Serial.println(F("--- Novy pokus: rozsviceni svetel ---"));
}

void sendDataResponse(WiFiClient& client) {
  client.println(F("HTTP/1.1 200 OK"));
  client.println(F("Content-Type: text/plain"));
  client.println(F("Access-Control-Allow-Origin: *"));
  client.println(F("Connection: close"));
  client.println();

  client.print(phaseLabel());
  client.print(',');
  client.print(lightsOn);
  client.print(',');
  client.print(lastValidMs);
  client.print(',');
  client.print(bestMs);
  client.print(',');
  client.print(validCount);
  client.print(',');
  client.print(falseStartCount);

  for (uint8_t i = 0; i < historyCount; i++) {
    client.print(',');
    client.print(historyMs[i]);
  }
  client.println();
}

void sendHtmlPage(WiFiClient& client) {
  client.println(F("HTTP/1.1 200 OK"));
  client.println(F("Content-Type: text/html; charset=UTF-8"));
  client.println(F("Connection: close"));
  client.println();

  client.println(F("<!DOCTYPE html><html><head><meta charset='UTF-8'>"));
  client.println(F("<meta name='viewport' content='width=device-width,initial-scale=1'>"));
  client.println(F("<title>Reakcni test</title><style>"));
  client.println(F("body{font-family:Segoe UI,sans-serif;background:#141414;color:#eee;text-align:center;padding:16px;}"));
  client.println(F("h1{margin:8px 0 4px;} .sub{color:#9aa;margin-bottom:22px;}"));
  client.println(F(".row{display:flex;flex-wrap:wrap;justify-content:center;gap:12px;}"));
  client.println(F(".card{background:#222;border:1px solid #333;border-radius:10px;padding:14px 22px;min-width:140px;}"));
  client.println(F(".card b{display:block;font-size:1.6em;color:#7cb7ff;margin-top:6px;}"));
  client.println(F(".lights{display:flex;justify-content:center;gap:10px;margin:24px 0;}"));
  client.println(F(".dot{width:28px;height:28px;border-radius:50%;background:#333;border:2px solid #555;}"));
  client.println(F(".dot.on{background:#e11;box-shadow:0 0 12px #e11;border-color:#f66;}"));
  client.println(F("#bars{max-width:420px;margin:18px auto;text-align:left;}"));
  client.println(F(".bar-row{display:flex;align-items:center;gap:8px;margin:6px 0;}"));
  client.println(F(".bar-row span{width:52px;font-size:0.9em;color:#aaa;}"));
  client.println(F(".bar{height:18px;background:#3a7;border-radius:4px;min-width:2px;}"));
  client.println(F("#status{font-size:1.15em;margin:10px 0;color:#fc6;}"));
  client.println(F("</style></head><body>"));
  client.println(F("<h1>Trenink pozornosti</h1>"));
  client.println(F("<p class='sub'>F1 startovaci svetla &middot; zive vysledky pres WiFi</p>"));
  client.println(F("<div id='status'>-</div>"));
  client.println(F("<div class='lights' id='lights'></div>"));
  client.println(F("<div class='row'>"));
  client.println(F("<div class='card'>Posledni cas<b id='last'>–</b></div>"));
  client.println(F("<div class='card'>Nejlepsi cas<b id='best'>–</b></div>"));
  client.println(F("<div class='card'>Platne pokusy<b id='ok'>0</b></div>"));
  client.println(F("<div class='card'>Predcasne<b id='fs'>0</b></div>"));
  client.println(F("</div>"));
  client.println(F("<h3>Historie casu</h3><div id='bars'></div>"));
  client.println(F("<script>"));
  client.println(F("const L=document.getElementById('lights');"));
  client.println(F("for(let i=0;i<5;i++){const d=document.createElement('div');d.className='dot';L.appendChild(d);}"));
  client.println(F("function loopData(){"));
  client.println(F(" fetch('/data').then(r=>r.text()).then(t=>{"));
  client.println(F("  const p=t.trim().split(',');"));
  client.println(F("  document.getElementById('status').textContent=p[0]||'-';"));
  client.println(F("  const on=+p[1]||0;"));
  client.println(F("  [...L.children].forEach((d,i)=>d.classList.toggle('on',i<on));"));
  client.println(F("  const last=+p[2]||0,best=+p[3]||0;"));
  client.println(F("  document.getElementById('last').textContent=last?last+' ms':'–';"));
  client.println(F("  document.getElementById('best').textContent=best?best+' ms':'–';"));
  client.println(F("  document.getElementById('ok').textContent=p[4]||'0';"));
  client.println(F("  document.getElementById('fs').textContent=p[5]||'0';"));
  client.println(F("  const hist=p.slice(6).map(Number).filter(n=>n>0);"));
  client.println(F("  const max=Math.max(1000,...hist);"));
  client.println(F("  const B=document.getElementById('bars'); B.innerHTML='';"));
  client.println(F("  hist.forEach((ms,i)=>{"));
  client.println(F("   const row=document.createElement('div'); row.className='bar-row';"));
  client.println(F("   row.innerHTML='<span>#'+(i+1)+'</span><div class=\"bar\" style=\"width:'+(ms/max*100)+'%\"></div><span>'+ms+' ms</span>';"));
  client.println(F("   B.appendChild(row);"));
  client.println(F("  });"));
  client.println(F(" }).catch(()=>{}).finally(()=>setTimeout(loopData,200));"));
  client.println(F("}"));
  client.println(F("loopData();"));
  client.println(F("</script></body></html>"));
}

void handleWebClient() {
  WiFiClient client = server.available();
  if (!client) return;

  String currentLine = "";
  bool isDataRequest = false;
  unsigned long startWait = millis();

  while (client.connected() && (millis() - startWait) < 800) {
    if (!client.available()) continue;

    char c = client.read();
    if (c == '\n') {
      if (currentLine.length() == 0) {
        if (isDataRequest) {
          sendDataResponse(client);
        } else {
          sendHtmlPage(client);
        }
        break;
      }
      currentLine = "";
    } else if (c != '\r') {
      currentLine += c;
      if (currentLine.endsWith("GET /data")) {
        isDataRequest = true;
      }
    }
  }
  client.stop();
}

void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.println(F("Trenink pozornosti / reakcni test (F1 svetla)"));
  Serial.println(F("Temata: WiFi komunikace + vizualizace dat (web)"));

  if (!IMU.begin()) {
    Serial.println(F("Chyba IMU!"));
    while (1);
  }

  Serial.print(F("Vytvarim AP: "));
  Serial.println(ssid);
  if (WiFi.beginAP(ssid, pass) != WL_AP_LISTENING) {
    Serial.println(F("Chyba AP!"));
    while (1);
  }
  delay(2000);
  server.begin();
  Serial.print(F("Web bezi na http://"));
  Serial.println(WiFi.localIP());
  Serial.println(F("Pripoj se na WiFi RP2040 / heslo ArduinoWeb"));

  float ax, ay, az;
  delay(50);
  if (IMU.accelerationAvailable()) {
    IMU.readAcceleration(ax, ay, az);
  } else {
    ax = ay = az = 0;
  }
  randomSeed((unsigned long)(micros() ^ (unsigned long)(ax * 1000) ^ (unsigned long)(ay * 1000)));

  allLightsOff();
  prevMag = readAccelMagnitude();
  phase = PHASE_START;
  phaseStartMs = millis();
  Serial.println(F(">>> Skubnutim spust prvni pokus."));
}

void loop() {
  unsigned long now = millis();
  bool jerked = detectJerk();

  switch (phase) {

    case PHASE_START:
      if (jerked) {
        startLighting();
      }
      break;

    case PHASE_LIGHTING:
      if (jerked) {
        phase = PHASE_FALSE_START;
        phaseStartMs = now;
        falseStartCount++;
        allLightsOff();
        Serial.println(F("PREDCASNA REAKCE behem rozsviceni!"));
        break;
      }

      if (lightsOn == 0 || (now - phaseStartMs) >= LIGHT_STEP_MS) {
        if (lightsOn < LIGHT_COUNT) {
          lightsOn++;
          showLights(lightsOn);
          phaseStartMs = now;
          Serial.print(F("Svetlo "));
          Serial.print(lightsOn);
          Serial.println(F("/5"));
        }
        if (lightsOn >= LIGHT_COUNT) {
          randomHoldMs = random(RANDOM_DELAY_MIN_MS, RANDOM_DELAY_MAX_MS + 1);
          phase = PHASE_WAIT_RANDOM;
          phaseStartMs = now;
          Serial.println(F("Vsech 5 svetel sviti. Cekam nahodnou dobu..."));
        }
      }
      break;

    case PHASE_WAIT_RANDOM:
      if (jerked) {
        phase = PHASE_FALSE_START;
        phaseStartMs = now;
        falseStartCount++;
        allLightsOff();
        Serial.println(F("PREDCASNA REAKCE pred zhasnutim!"));
        break;
      }

      if ((now - phaseStartMs) >= randomHoldMs) {
        allLightsOff();
        lightsOn = 0;
        reactionStartMs = millis();
        phase = PHASE_MEASURE;
        imuReady = false;
        prevMag = readAccelMagnitude();
        Serial.println(F("ZHASNUTO! Skubni co nejdriv..."));
      }
      break;

    case PHASE_MEASURE:
      if (jerked) {
        reactionTimeMs = millis() - reactionStartMs;
        recordValidResult(reactionTimeMs);
        phase = PHASE_RESULT;
        phaseStartMs = now;

        for (uint8_t i = 0; i < LIGHT_COUNT; i++) {
          np.neoPixelSetValue(LIGHT_LEDS[i], 0, 120, 0, false);
        }
        np.neoPixelShow();
        lightsOn = LIGHT_COUNT;

        Serial.print(F("Reakcni cas: "));
        Serial.print(reactionTimeMs);
        Serial.println(F(" ms"));
      }
      break;

    case PHASE_RESULT:
    case PHASE_FALSE_START:
      if ((now - phaseStartMs) >= RESULT_HOLD_MS) {
        resetTrialToStart();
      }
      break;
  }

  if (phase != PHASE_MEASURE) {
    handleWebClient();
  }
}
