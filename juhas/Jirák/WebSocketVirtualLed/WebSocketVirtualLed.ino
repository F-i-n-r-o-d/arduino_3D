/*
  Rehabko – náklonová hra s kuličkou na LED pásku + webový žebříček
  Deska: Arduino Nano RP2040 Connect (LSM6DSOX, WiFiNINA), 64× NeoPixel na pinu 4

  Princip:
  - Každý level má barvu pozadí (modrá = klid → červená = finále)
    a vyšší "náchylnost" kuličky k náklonu (zrychlení + max. rychlost).
  - V levelu jsou 3 cíle. Kulička musí v cíli vydržet DOBA_V_CILI ms,
    pak se cíl přesune. Po 3 cílech se jde do dalšího levelu.
  - Po zvládnutí červeného levelu celý pásek bliká zeleně = konec.
  - Na webu (Wi-Fi "RP2040", http://192.168.4.1) se zadá jméno a spustí hra.
    Měří se celkový čas a ukládá se žebříček (v RAM – po resetu desky se smaže).
*/

#include <NeoPixelConnect.h>
#include <Arduino_LSM6DSOX.h>
#include <WiFiNINA.h>
#include <SPI.h>

// ---------------- Nastavení ----------------
#define POCET_LED 64
#define LED_PIN 4

const int SIRKA_CILE = 8;                 // počet diod cíle
const unsigned long DOBA_V_CILI = 5000;   // jak dlouho musí kulička vydržet v cíli [ms]
const int CILU_NA_LEVEL = 3;              // kolik přesunů cíle na jeden level
const float MRTVA_ZONA = 0.03;            // malý náklon [g] se ignoruje (kulička nedrifuje)
const float TRENI = 1.8;                  // tlumení rychlosti (větší = kulička víc brzdí)
const float JAS_POZADI = 0.08;            // jas pozadí na pásku (0–1)

char ssid[] = "RP2040";
char pass[] = "ArduinoWeb";

// Levely: barva pozadí (plný jas) + citlivost [diod/s² na 1 g] + max. rychlost [diod/s]
struct Level {
  uint8_t r, g, b;
  float citlivost;
  float maxRychlost;
};

const Level LEVELY[] = {
  {   0,   0, 255,  25, 12 },   // 1 tmavě modrá – klid
  {   0, 100, 255,  45, 20 },   // 2 světlejší modrá
  {   0, 200, 255,  65, 28 },   // 3 světle modrá
  { 140,   0, 255,  85, 36 },   // 4 fialová
  { 255,   0, 140, 105, 45 },   // 5 purpurová
  { 255,  80,   0, 125, 54 },   // 6 oranžová
  { 255,   0,   0, 150, 65 },   // 7 červená – finále
};
const int POCET_LEVELU = sizeof(LEVELY) / sizeof(LEVELY[0]);

// ---------------- Stav hry ----------------
enum Stav { CEKANI = 0, HRA = 1, KONEC = 2 };
Stav stav = CEKANI;

NeoPixelConnect np(LED_PIN, POCET_LED, pio0, 0);
WiFiServer server(80);

float x = 0, y = 0, z = 0;
float pozice = POCET_LED / 2;   // poloha kuličky (desetinná kvůli plynulému pohybu)
float rychlost = 0;
int kulickaLed = POCET_LED / 2;

int level = 0;                  // 0 .. POCET_LEVELU-1
int cilHotovo = 0;              // kolik cílů v levelu je hotových
int cilStart = 0;               // první dioda cíle
unsigned long vCiliMs = 0;      // jak dlouho už je kulička v cíli

String jmenoHrace = "";
unsigned long startHry = 0;
unsigned long konecnyCas = 0;

unsigned long posledniFyzika = 0;
unsigned long posledniLed = 0;

// ---------------- Žebříček ----------------
const int MAX_ZEBRICEK = 10;
struct Zaznam { String jmeno; unsigned long cas; };
Zaznam zebricek[MAX_ZEBRICEK];
int pocetZaznamu = 0;

void pridejDoZebricku(const String& jmeno, unsigned long cas) {
  // najdi místo (seřazeno od nejrychlejšího)
  int i = pocetZaznamu;
  if (i == MAX_ZEBRICEK) {
    if (cas >= zebricek[MAX_ZEBRICEK - 1].cas) return;  // nevešel se
    i = MAX_ZEBRICEK - 1;
  } else {
    pocetZaznamu++;
  }
  while (i > 0 && zebricek[i - 1].cas > cas) {
    zebricek[i] = zebricek[i - 1];
    i--;
  }
  zebricek[i].jmeno = jmeno;
  zebricek[i].cas = cas;
}

// ---------------- Herní logika ----------------
void novyCil() {
  // nový cíl dostatečně daleko od kuličky
  int novy = 0;
  for (int pokus = 0; pokus < 30; pokus++) {
    novy = random(0, POCET_LED - SIRKA_CILE + 1);
    int stredCile = novy + SIRKA_CILE / 2;
    if (abs(stredCile - kulickaLed) >= 16) break;
  }
  cilStart = novy;
  vCiliMs = 0;
}

void spustHru(const String& jmeno) {
  jmenoHrace = jmeno;
  randomSeed(micros());
  level = 0;
  cilHotovo = 0;
  pozice = POCET_LED / 2;
  rychlost = 0;
  kulickaLed = (int)pozice;
  konecnyCas = 0;
  novyCil();
  startHry = millis();
  posledniFyzika = millis();
  stav = HRA;
}

void aktualizujHru() {
  unsigned long ted = millis();
  float dt = (ted - posledniFyzika) / 1000.0;
  posledniFyzika = ted;
  if (dt > 0.1) dt = 0.1;   // ochrana po pauze (např. při posílání webu)

  if (IMU.accelerationAvailable()) {
    IMU.readAcceleration(x, y, z);
  }

  if (stav != HRA) return;

  const Level& L = LEVELY[level];

  // fyzika: náklon → zrychlení, tření, omezení rychlosti
  float naklon = x;
  if (fabs(naklon) < MRTVA_ZONA) naklon = 0;
  rychlost += naklon * L.citlivost * dt;
  rychlost -= rychlost * TRENI * dt;
  if (rychlost >  L.maxRychlost) rychlost =  L.maxRychlost;
  if (rychlost < -L.maxRychlost) rychlost = -L.maxRychlost;

  pozice += rychlost * dt;
  if (pozice < 0)              { pozice = 0;              rychlost = 0; }
  if (pozice > POCET_LED - 1)  { pozice = POCET_LED - 1;  rychlost = 0; }
  kulickaLed = (int)(pozice + 0.5);

  // je kulička v cíli?
  bool vCili = (kulickaLed >= cilStart && kulickaLed < cilStart + SIRKA_CILE);
  if (vCili) {
    vCiliMs += (unsigned long)(dt * 1000);
  } else {
    vCiliMs = 0;   // musí tam vydržet bez přerušení
  }

  if (vCiliMs >= DOBA_V_CILI) {
    cilHotovo++;
    if (cilHotovo >= CILU_NA_LEVEL) {
      cilHotovo = 0;
      level++;
      if (level >= POCET_LEVELU) {
        level = POCET_LEVELU - 1;
        konecnyCas = millis() - startHry;
        pridejDoZebricku(jmenoHrace, konecnyCas);
        stav = KONEC;
        return;
      }
      rychlost = 0;   // nový level začíná z klidu
    }
    novyCil();
  }
}

// ---------------- LED pásek ----------------
void vykresliPasek() {
  unsigned long ted = millis();
  if (ted - posledniLed < 15) return;   // cca 60 snímků/s stačí
  posledniLed = ted;

  if (stav == KONEC) {
    bool sviti = (ted / 400) % 2 == 0;
    for (int i = 0; i < POCET_LED; i++) {
      np.neoPixelSetValue(i, 0, sviti ? 120 : 0, 0, false);
    }
    np.neoPixelShow();
    return;
  }

  const Level& L = LEVELY[stav == HRA ? level : 0];
  uint8_t bgR = L.r * JAS_POZADI, bgG = L.g * JAS_POZADI, bgB = L.b * JAS_POZADI;

  // kolik diod cíle už "nabito" (ukazatel 5 s)
  int nabito = (int)((float)vCiliMs / DOBA_V_CILI * SIRKA_CILE);

  for (int i = 0; i < POCET_LED; i++) {
    if (stav == HRA && i == kulickaLed) {
      np.neoPixelSetValue(i, 150, 150, 150, false);          // bílá kulička
    } else if (stav == HRA && i >= cilStart && i < cilStart + SIRKA_CILE) {
      if (i - cilStart < nabito) np.neoPixelSetValue(i, 0, 120, 0, false);  // nabitá část cíle
      else                       np.neoPixelSetValue(i, 0, 25, 0, false);   // zelený cíl
    } else {
      np.neoPixelSetValue(i, bgR, bgG, bgB, false);          // pozadí levelu
    }
  }
  np.neoPixelShow();
}

// ---------------- Webová stránka ----------------
const char STRANKA[] = R"rawliteral(<!DOCTYPE html>
<html lang="cs">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Rehabko Hra</title>
<style>
  :root { --lvl: #0000ff; }
  * { box-sizing: border-box; }
  body {
    margin: 0; padding: 20px 16px 40px;
    font-family: "Segoe UI", sans-serif;
    background: #161618; color: #eee; text-align: center;
  }
  h1 { margin: 6px 0 2px; }
  .sub { color: #8b949e; margin: 0 0 18px; }
  .wrap { max-width: 900px; margin: 0 auto; }
  .panel {
    background: #21262d; border: 1px solid #30363d; border-radius: 12px;
    padding: 16px; margin: 14px 0;
  }
  #start input {
    font: inherit; padding: 10px 14px; border-radius: 8px; width: 220px;
    border: 1px solid #30363d; background: #0d1117; color: #eee;
  }
  button {
    font: inherit; font-weight: 600; padding: 10px 20px; border-radius: 8px;
    border: none; cursor: pointer; margin: 4px;
  }
  #btnStart { background: #238636; color: #fff; }
  #btnStop { background: #30363d; color: #eee; display: none; }
  button:disabled { opacity: .4; cursor: default; }

  #pasek {
    display: flex; gap: 2px; padding: 8px; border-radius: 12px;
    background: #0d1117; border: 2px solid var(--lvl);
    box-shadow: 0 0 18px var(--lvl); transition: border-color .4s, box-shadow .4s;
  }
  .led { flex: 1; height: 34px; border-radius: 3px; transition: background .08s; }
  .led.kulicka { background: #fff !important; box-shadow: 0 0 10px #fff; }
  .blik .led { animation: blik .8s steps(1) infinite; }
  @keyframes blik { 0% { background: #2ea043; } 50% { background: #0d1117; } }

  .stats { display: flex; flex-wrap: wrap; justify-content: center; gap: 12px; }
  .card {
    background: #21262d; border: 1px solid #30363d; border-radius: 10px;
    padding: 12px 22px; min-width: 140px;
  }
  .card p { margin: 0 0 4px; color: #8b949e; font-size: .9em; }
  .card b { font-size: 1.5em; color: #58a6ff; }
  #cas { font-variant-numeric: tabular-nums; }
  .bar { height: 8px; background: #0d1117; border-radius: 4px; overflow: hidden; margin-top: 8px; }
  #barIn { height: 100%; width: 0; background: #2ea043; }

  #zprava { font-size: 1.2em; min-height: 1.5em; margin: 10px 0; }
  #status { font-size: .85em; color: #8b949e; }
  #status.err { color: #f85149; }

  table { width: 100%; border-collapse: collapse; }
  th, td { padding: 8px; border-bottom: 1px solid #30363d; }
  th { color: #8b949e; font-weight: 600; }
  td:nth-child(3) { font-variant-numeric: tabular-nums; }
  tr.prvni td { color: #e3b341; font-weight: 700; }
</style>
</head>
<body>
<div class="wrap">
  <h1>Rehabilitační pomůcka</h1>
  <p class="sub">Náklonová hra – od klidné modré po ohnivou červenou</p>

  <div class="panel" id="start">
    <input id="jmeno" maxlength="16" placeholder="Zadej své jméno">
    <button id="btnStart">Start</button>
    <button id="btnStop">Ukončit hru</button>
    <div id="status">Připojuji…</div>
  </div>

  <div id="zprava"></div>
  <div id="pasek"></div>

  <div class="stats">
    <div class="card"><p>Hráč</p><b id="hrac">–</b></div>
    <div class="card"><p>Level</p><b id="lvl">–</b></div>
    <div class="card"><p>Cíl</p><b id="cil">–</b>
      <div class="bar"><div id="barIn"></div></div></div>
    <div class="card"><p>Čas</p><b id="cas">0:00.0</b></div>
  </div>

  <div class="panel">
    <h2>Žebříček</h2>
    <table>
      <thead><tr><th>#</th><th>Jméno</th><th>Čas</th></tr></thead>
      <tbody id="zebricek"><tr><td colspan="3">Zatím nikdo nedohrál</td></tr></tbody>
    </table>
  </div>
</div>

<script>
const $ = id => document.getElementById(id);
const N = 64;
const pasek = $('pasek');
const ledky = [];
for (let i = 0; i < N; i++) {
  const d = document.createElement('div');
  d.className = 'led';
  pasek.appendChild(d);
  ledky.push(d);
}

function formatCas(ms) {
  const s = ms / 1000;
  const m = Math.floor(s / 60);
  const zb = (s % 60).toFixed(1).padStart(4, '0');
  return m + ':' + zb;
}

function hexNaRgba(hex, a) {
  const r = parseInt(hex.substr(1, 2), 16), g = parseInt(hex.substr(3, 2), 16), b = parseInt(hex.substr(5, 2), 16);
  return `rgba(${r},${g},${b},${a})`;
}

let predchoziStav = -1;

function vykresli(d) {
  document.documentElement.style.setProperty('--lvl', d.s === 2 ? '#2ea043' : d.bg);
  pasek.classList.toggle('blik', d.s === 2);

  const pozadi = hexNaRgba(d.bg, 0.35);
  const nabito = Math.floor(d.h / 1000 * d.w);
  for (let i = 0; i < N; i++) {
    const el = ledky[i];
    el.classList.toggle('kulicka', d.s === 1 && i === d.b);
    if (d.s === 2) { el.style.background = ''; continue; }
    if (d.s === 1 && i >= d.c && i < d.c + d.w) {
      el.style.background = (i - d.c < nabito) ? '#2ea043' : 'rgba(46,160,67,0.3)';
    } else {
      el.style.background = pozadi;
    }
  }

  $('hrac').textContent = d.j || '–';
  $('lvl').textContent = d.s === 0 ? '–' : d.l + ' / ' + d.ln;
  $('cil').textContent = d.s === 1 ? d.k + ' / ' + d.kn : '–';
  $('barIn').style.width = (d.s === 1 ? d.h / 10 : 0) + '%';
  $('cas').textContent = formatCas(d.cas);

  const hraje = d.s === 1;
  $('btnStart').disabled = hraje;
  $('jmeno').disabled = hraje;
  $('btnStop').style.display = hraje ? 'inline-block' : 'none';

  if (d.s === 0) $('zprava').textContent = 'Zadej jméno a stiskni Start';
  else if (d.s === 1) $('zprava').textContent = d.l === d.ln ? 'Finální level!' : 'Dojeď kuličkou do zeleného cíle a vydrž 5 s';
  else $('zprava').textContent = '🎉 Hotovo! ' + d.j + ' to zvládl/a za ' + formatCas(d.cas);

  if (d.s === 2 && predchoziStav !== 2) nactiZebricek();
  predchoziStav = d.s;
}

function smycka() {
  fetch('/data')
    .then(r => r.json())
    .then(d => {
      vykresli(d);
      $('status').textContent = 'Připojeno';
      $('status').className = '';
    })
    .catch(() => {
      $('status').textContent = 'Bez spojení s Arduinem';
      $('status').className = 'err';
    })
    .finally(() => setTimeout(smycka, 60));
}

function nactiZebricek() {
  fetch('/zebricek').then(r => r.json()).then(z => {
    const tb = $('zebricek');
    tb.innerHTML = '';
    if (z.length === 0) {
      tb.innerHTML = '<tr><td colspan="3">Zatím nikdo nedohrál</td></tr>';
      return;
    }
    z.forEach((r, i) => {
      const tr = document.createElement('tr');
      if (i === 0) tr.className = 'prvni';
      [String(i + 1) + '.', r.j, formatCas(r.t)].forEach(t => {
        const td = document.createElement('td');
        td.textContent = t;
        tr.appendChild(td);
      });
      tb.appendChild(tr);
    });
  }).catch(() => {});
}

$('btnStart').onclick = () => {
  const j = $('jmeno').value.trim();
  if (!j) { $('jmeno').focus(); $('zprava').textContent = 'Nejdřív napiš jméno'; return; }
  fetch('/start?jmeno=' + encodeURIComponent(j));
};
$('jmeno').addEventListener('keydown', e => { if (e.key === 'Enter') $('btnStart').click(); });
$('btnStop').onclick = () => fetch('/stop');

smycka();
nactiZebricek();
setInterval(nactiZebricek, 5000);
</script>
</body>
</html>
)rawliteral";

// ---------------- Web server ----------------
void posliText(WiFiClient& c, const char* s) {
  // velké texty po kouscích, ať je WiFi modul zvládne
  size_t n = strlen(s), i = 0;
  while (i < n) {
    size_t k = n - i;
    if (k > 1000) k = 1000;
    c.write((const uint8_t*)s + i, k);
    i += k;
  }
}

void hlavicka(WiFiClient& c, const char* typ, const char* kod = "200 OK") {
  c.print("HTTP/1.1 "); c.print(kod);
  c.print("\r\nContent-Type: "); c.print(typ);
  c.print("\r\nAccess-Control-Allow-Origin: *\r\nCache-Control: no-store\r\nConnection: close\r\n\r\n");
}

String urlDecode(const String& s) {
  String out = "";
  for (unsigned int i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '+') out += ' ';
    else if (c == '%' && i + 2 < s.length()) {
      char hex[3] = { s[i + 1], s[i + 2], 0 };
      out += (char)strtol(hex, NULL, 16);
      i += 2;
    } else out += c;
  }
  return out;
}

String vycistiJmeno(const String& s) {
  // pryč se znaky, které by rozbily JSON/HTML; diakritika (UTF-8) zůstává
  String out = "";
  for (unsigned int i = 0; i < s.length(); i++) {
    unsigned char c = s[i];
    if (c < 32 || c == '"' || c == '\\' || c == '<' || c == '>' || c == '&') continue;
    out += (char)c;
  }
  out.trim();
  if (out.length() > 48) out = out.substring(0, 48);
  if (out.length() == 0) out = "Hráč";
  return out;
}

void posliData(WiFiClient& c) {
  const Level& L = LEVELY[stav == CEKANI ? 0 : level];
  unsigned long cas = 0;
  if (stav == HRA) cas = millis() - startHry;
  else if (stav == KONEC) cas = konecnyCas;

  int h = (stav == HRA) ? (int)(vCiliMs * 1000UL / DOBA_V_CILI) : 0;
  if (h > 1000) h = 1000;

  char buf[200];
  snprintf(buf, sizeof(buf),
    "{\"s\":%d,\"x\":%.2f,\"b\":%d,\"c\":%d,\"w\":%d,\"l\":%d,\"ln\":%d,"
    "\"k\":%d,\"kn\":%d,\"h\":%d,\"cas\":%lu,\"bg\":\"#%02X%02X%02X\",\"j\":\"",
    (int)stav, x, kulickaLed, cilStart, SIRKA_CILE, level + 1, POCET_LEVELU,
    cilHotovo + 1, CILU_NA_LEVEL, h, cas, L.r, L.g, L.b);

  hlavicka(c, "application/json; charset=UTF-8");
  c.print(buf);
  c.print(jmenoHrace);
  c.print("\"}");
}

void posliZebricek(WiFiClient& c) {
  hlavicka(c, "application/json; charset=UTF-8");
  c.print("[");
  for (int i = 0; i < pocetZaznamu; i++) {
    if (i) c.print(",");
    c.print("{\"j\":\""); c.print(zebricek[i].jmeno);
    c.print("\",\"t\":"); c.print(zebricek[i].cas);
    c.print("}");
  }
  c.print("]");
}

void obsluzWeb() {
  WiFiClient client = server.available();
  if (!client) return;

  String radek = "", pozadavek = "";
  bool prvni = true;
  unsigned long t0 = millis();

  // přečti hlavičky až po prázdný řádek, zapamatuj si první řádek
  while (client.connected() && millis() - t0 < 1000) {
    if (!client.available()) continue;
    char c = client.read();
    if (c == '\n') {
      if (radek.length() == 0) break;
      if (prvni) { pozadavek = radek; prvni = false; }
      radek = "";
    } else if (c != '\r') {
      radek += c;
    }
  }

  // "GET /cesta HTTP/1.1" → "/cesta"
  int a = pozadavek.indexOf(' ');
  int b = pozadavek.indexOf(' ', a + 1);
  String cesta = (a >= 0 && b > a) ? pozadavek.substring(a + 1, b) : "/";

  if (cesta == "/data") {
    posliData(client);
  } else if (cesta == "/zebricek") {
    posliZebricek(client);
  } else if (cesta.startsWith("/start")) {
    String jmeno = "";
    int p = cesta.indexOf("jmeno=");
    if (p >= 0) jmeno = cesta.substring(p + 6);
    int amp = jmeno.indexOf('&');
    if (amp >= 0) jmeno = jmeno.substring(0, amp);
    spustHru(vycistiJmeno(urlDecode(jmeno)));
    hlavicka(client, "text/plain");
    client.print("OK");
  } else if (cesta == "/stop") {
    stav = CEKANI;
    hlavicka(client, "text/plain");
    client.print("OK");
  } else if (cesta == "/" || cesta.startsWith("/?")) {
    hlavicka(client, "text/html; charset=UTF-8");
    posliText(client, STRANKA);
  } else {
    hlavicka(client, "text/plain", "404 Not Found");
    client.print("Nenalezeno");
  }

  delay(1);
  client.stop();
}

// ---------------- Setup / loop ----------------
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

  posledniFyzika = millis();
}

void loop() {
  aktualizujHru();
  vykresliPasek();
  obsluzWeb();
}
