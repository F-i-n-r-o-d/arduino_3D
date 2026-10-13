#include <NeoPixelConnect.h>
#include <Arduino_LSM6DSOX.h>
#include <WiFiNINA.h>
#include <SPI.h>

// Puvodni hardware: 64 LED na D4, akcelerometr LSM6DSOX.
#define LED_COUNT 64
#define TARGET_WIDTH 10
#define HOLD_MS 2000UL
NeoPixelConnect np(4, LED_COUNT, pio0, 0);
char ssid[] = "RP2040";
char pass[] = "ArduinoWeb";
WiFiServer server(80);

// Stejna stranka jako nahled.html, vlozena do programu pro Arduino.
const char PAGE_HTML[] = R"REHABPAGE(<!doctype html>
<html lang="cs"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1">
<title>Rehabko Monitor – Terč</title>
<style>
:root{font-family:Segoe UI,Arial,sans-serif;color:#eee;background:#181818;color-scheme:dark}
*{box-sizing:border-box}body{text-align:center;margin:0;padding:22px 12px}main{max-width:850px;margin:auto}h1{margin:10px 0 5px;font-size:clamp(24px,4vw,34px)}p{color:#aeb7c4}.panel{background:#21262d;border:1px solid #30363d;border-radius:12px;padding:20px;margin:16px 0}.status{font-size:13px;color:#9ba9bc;margin:4px 0 20px}.track{position:relative;display:grid;grid-template-columns:repeat(64,minmax(0,1fr));gap:2px;width:100%;height:50px;padding:5px;background:#0d1117;border:2px solid #30363d;border-radius:12px;margin:24px auto 30px}.led{border-radius:3px;background:#182b48;min-width:0}.led.p1{background:#299154}.led.p3{background:#d09a27}.led.p5{background:#ffe069}.led.ball{background:#fa5553!important;box-shadow:0 0 9px #ff5656;z-index:1}.legend{display:flex;gap:18px;justify-content:center;flex-wrap:wrap;font-size:13px;color:#bdc7d3}.dot{display:inline-block;width:12px;height:12px;margin-right:5px;border-radius:3px;vertical-align:middle}.stats{display:grid;grid-template-columns:repeat(4,1fr);gap:10px}.stat{background:#21262d;border:1px solid #30363d;border-radius:10px;padding:13px 6px;font-size:13px;color:#b9c3cf}.stat strong{display:block;font-size:27px;color:#58a6ff;margin:5px 0}progress{width:100%;height:16px;accent-color:#36b96e}.progress-wrap{max-width:560px;margin:auto}button{background:#2e7adb;border:0;border-radius:8px;padding:12px 19px;color:white;font-size:15px;cursor:pointer}button:hover{background:#428bed}button:focus-visible{outline:2px solid white}.controls{display:flex;justify-content:center;gap:12px;flex-wrap:wrap;margin:16px}.hint{font-size:13px;color:#aeb7c4}.result{font-size:18px;font-weight:650;min-height:26px}.demo-only{display:none}body.demo .demo-only{display:block}@media(max-width:600px){.stats{grid-template-columns:repeat(2,1fr)}.track{gap:1px;height:42px;padding:3px}.panel{padding:12px}.stat strong{font-size:23px}}
</style></head><body><main>
<h1>Rehabilitační pomůcka – terč</h1><p>Čím přesněji udržíš červenou diodu ve středu, tím více bodů získáš.</p>
<div class="status" id="mode">Načítám připojení…</div>
<section class="panel"><div class="track" id="track" aria-label="LED pásek se 64 diodami"></div>
<div class="legend"><span><i class="dot" style="background:#299154"></i>1 bod</span><span><i class="dot" style="background:#d09a27"></i>3 body</span><span><i class="dot" style="background:#ffe069"></i>5 bodů</span><span><i class="dot" style="background:#fa5553"></i>Aktuální poloha</span></div></section>
<section class="stats"><div class="stat">Skóre<strong id="score">0</strong></div><div class="stat">Zásahy<strong id="hits">0</strong></div><div class="stat">Poloha diody<strong id="position">0 / 63</strong></div><div class="stat">Náklon X<strong id="tilt">0.00 g</strong></div></section>
<section class="panel progress-wrap"><div id="zone" class="result">Najdi cílové pole</div><p>Vydrž v terči 2 sekundy</p><progress id="hold" max="2000" value="0"></progress><p id="holdText">0,0 / 2,0 s</p><div id="last">Poslední zásah: –</div></section>
<section class="panel demo-only"><h2>Zkušební režim bez Arduina</h2><p>Podrž šipky ← a → nebo použij tlačítka. Pohyb simuluje náklon tyče.</p><div class="controls"><button id="left" type="button">◀ Vlevo</button><button id="right" type="button">Vpravo ▶</button><button id="restart" type="button">Nová hra</button></div><p class="hint">Terče se po úspěšném dvousekundovém držení mění automaticky.</p></section>
</main><script>
'use strict';
const TARGET_WIDTH=10,HOLD_MS=2000,LED_COUNT=64;
const colors=[1,1,3,3,5,5,3,3,1,1];
const isDemo=location.protocol==='file:' || location.hostname==='localhost' || location.hostname==='127.0.0.1' || new URLSearchParams(location.search).has('demo');
const el=id=>document.getElementById(id);
const leds=[];
for(let i=0;i<LED_COUNT;i++){const led=document.createElement('div');led.className='led';el('track').appendChild(led);leds.push(led)}
let state={x:0,pos:31,target:25,score:0,hits:0,hold:0,last:0};
function points(pos,target){let offset=pos-target;return offset>=0&&offset<TARGET_WIDTH?colors[offset]:0}
function render(s){
  for(let i=0;i<LED_COUNT;i++){let pts=points(i,s.target);leds[i].className='led'+(pts?' p'+pts:'')+(i===s.pos?' ball':'')}
  el('score').textContent=s.score;el('hits').textContent=s.hits;
  el('position').textContent=s.pos+' / 63';el('tilt').textContent=Number(s.x).toFixed(2)+' g';
  el('zone').textContent=points(s.pos,s.target)?'Aktuálně: '+points(s.pos,s.target)+' bodů':'Najdi cílové pole';
  el('hold').value=Math.max(0,Math.min(HOLD_MS,s.hold));
  el('holdText').textContent=(s.hold/1000).toFixed(1).replace('.',',')+' / 2,0 s';
  el('last').textContent='Poslední zásah: '+(s.last?s.last+' bodů':'–');
}
function nextTarget(old){let next=Math.floor(Math.random()*(LED_COUNT-TARGET_WIDTH+1));if(next===old)next=(next+1)%(LED_COUNT-TARGET_WIDTH+1);return next}
function simulate(dt){if(points(state.pos,state.target)){
  state.hold+=dt;
  if(state.hold>=HOLD_MS){state.last=points(state.pos,state.target);state.score+=state.last;state.hits++;state.target=nextTarget(state.target);state.hold=0}
 }else state.hold=0;
 state.x=(state.pos/63-0.5);render(state);
}
if(isDemo){
 document.body.classList.add('demo');el('mode').textContent='DEMO – bez připojeného Arduina';
 let moving=0;const setMove=n=>{moving=n};
 document.addEventListener('keydown',e=>{if(e.key==='ArrowLeft'||e.key==='ArrowRight'){e.preventDefault();moving=e.key==='ArrowLeft'?-1:1}});
 document.addEventListener('keyup',e=>{if(e.key==='ArrowLeft'&&moving===-1||e.key==='ArrowRight'&&moving===1)moving=0});
 window.addEventListener('blur',()=>{moving=0});
 for(const [id,dir] of [['left',-1],['right',1]]){let b=el(id);b.addEventListener('pointerdown',e=>{e.preventDefault();setMove(dir);b.setPointerCapture(e.pointerId)});b.addEventListener('pointerup',()=>setMove(0));b.addEventListener('pointercancel',()=>setMove(0));b.addEventListener('lostpointercapture',()=>setMove(0))}
 el('restart').addEventListener('click',()=>{state={x:0,pos:31,target:nextTarget(-1),score:0,hits:0,hold:0,last:0};render(state)});
 let prev=performance.now(),moveAcc=0;
 function frame(now){let dt=Math.min(100,now-prev);prev=now;if(moving){moveAcc+=dt;while(moveAcc>=45){state.pos=Math.max(0,Math.min(63,state.pos+moving));moveAcc-=45}}else moveAcc=0;simulate(dt);requestAnimationFrame(frame)}
 render(state);requestAnimationFrame(frame);
}else{
 el('mode').textContent='Živý režim – data z Arduina';
 async function update(){try{const r=await fetch('/data',{cache:'no-store'});if(!r.ok)throw Error('HTTP '+r.status);const a=(await r.text()).trim().split(',').map(Number);if(a.length!==8||a.some(v=>!Number.isFinite(v)))throw Error('Neplatná data');
 state={x:a[0],pos:a[1],target:a[2],score:a[3],hits:a[4],hold:a[5],last:a[6]}; // a[7] = číslo aktuální hry (rezerva)
 render(state);el('mode').textContent='Živý režim – připojeno k Arduinu';
 }catch(e){el('mode').textContent='Čekám na data z Arduina…'}finally{setTimeout(update,120)}}update();
}
</script></body></html>
)REHABPAGE";

int targetStart = 0;
int positionLed = 31;
float x = 0, y = 0, z = 0;
unsigned long holdStart = 0;
unsigned long holdDuration = 0;
unsigned long score = 0;
unsigned int hits = 0;
int lastPoints = 0;
unsigned long lastLedUpdate = 0;

int scoreForPosition(int pos, int start) {
  int d = pos - start;
  if (d < 0 || d >= TARGET_WIDTH) return 0;
  if (d == 4 || d == 5) return 5;
  if (d >= 2 && d <= 7) return 3;
  return 1;
}

int nextTarget(int oldStart) {
  int next = random(0, LED_COUNT - TARGET_WIDTH + 1);  // 0..54 vcetne
  if (next == oldStart) next = (next + 1) % (LED_COUNT - TARGET_WIDTH + 1);
  return next;
}

void renderLeds() {
  for (int i = 0; i < LED_COUNT; i++) {
    int pts = scoreForPosition(i, targetStart);
    if (i == positionLed) np.neoPixelSetValue((uint8_t)i, 120, 0, 0, true);
    else if (pts == 5) np.neoPixelSetValue((uint8_t)i, 65, 42, 0, true);
    else if (pts == 3) np.neoPixelSetValue((uint8_t)i, 22, 12, 0, true);
    else if (pts == 1) np.neoPixelSetValue((uint8_t)i, 0, 22, 0, true);
    else np.neoPixelSetValue((uint8_t)i, 0, 0, 10, true);
  }
}

void updateGame() {
  if (IMU.accelerationAvailable()) IMU.readAcceleration(x, y, z);
  int p = (int)((x + 0.5f) * 63.0f);
  positionLed = constrain(p, 0, LED_COUNT - 1);
  unsigned long now = millis();
  int currentPoints = scoreForPosition(positionLed, targetStart);
  if (currentPoints == 0) {
    holdStart = 0;
    holdDuration = 0;
  } else {
    if (holdStart == 0) holdStart = now ? now : 1;
    holdDuration = now - holdStart;
    if (holdDuration >= HOLD_MS) {
      lastPoints = currentPoints;
      score += currentPoints;
      hits++;
      targetStart = nextTarget(targetStart);
      holdStart = 0;
      holdDuration = 0;
    }
  }
  // Nesvitit kazdou iteraci weboveho serveru, max 25x za sekundu.
  if (now - lastLedUpdate >= 40) {
    renderLeds();
    lastLedUpdate = now;
  }
}

void respond(WiFiClient &client, bool dataRequest) {
  client.println("HTTP/1.1 200 OK");
  if (dataRequest) {
    client.println("Content-Type: text/plain; charset=UTF-8");
    client.println("Cache-Control: no-store");
  } else client.println("Content-Type: text/html; charset=UTF-8");
  client.println("Connection: close");
  client.println();
  if (dataRequest) {
    // Format: x, pozice, cil, skore, zasahy, drzeniMs, posledniBody, rezerva
    client.print(x, 2); client.print(',');
    client.print(positionLed); client.print(',');
    client.print(targetStart); client.print(',');
    client.print(score); client.print(',');
    client.print(hits); client.print(',');
    client.print(holdDuration); client.print(',');
    client.print(lastPoints); client.println(",0");
  } else client.print(PAGE_HTML);
}

void handleHttp() {
  WiFiClient client = server.available();
  if (!client) return;
  // Ochrana proti klientovi, ktery neposle data a zablokuje cteni IMU.
  unsigned long started = millis();
  String request = "";
  while (client.connected() && millis() - started < 400) {
    updateGame();
    while (client.available()) {
      char c = client.read();
      if (c == '\n') {
        bool dataRequest = request.startsWith("GET /data ");
        respond(client, dataRequest);
        client.stop();
        return;
      }
      if (c != '\r' && request.length() < 100) request += c;
    }
    delay(1);
  }
  client.stop();
}

void setup() {
  Serial.begin(115200);
  delay(1500);
  if (!IMU.begin()) { Serial.println("Chyba IMU!"); while (1) delay(100); }
  randomSeed((unsigned long)(IMU.accelerationAvailable() ? micros() : millis()));
  targetStart = random(0, LED_COUNT - TARGET_WIDTH + 1);
  if (WiFi.beginAP(ssid, pass) != WL_AP_LISTENING) {
    Serial.println("Chyba vytvoreni Wi-Fi AP!");
    while (1) delay(100);
  }
  server.begin();
  Serial.print("Web: http://");
  Serial.println(WiFi.localIP());
  renderLeds();
}

void loop() {
  updateGame();
  handleHttp();
  delay(5);
}
