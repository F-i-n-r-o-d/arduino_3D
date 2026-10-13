const LIGHT_COUNT = 5;
const LIGHT_STEP_MS = 500;
const RANDOM_DELAY_MIN_MS = 500;
const RANDOM_DELAY_MAX_MS = 5000;
const RESULT_HOLD_MS = 2500;
const HISTORY_SIZE = 8;

const PHASE_START = "PHASE_START";
const PHASE_LIGHTING = "PHASE_LIGHTING";
const PHASE_WAIT_RANDOM = "PHASE_WAIT_RANDOM";
const PHASE_MEASURE = "PHASE_MEASURE";
const PHASE_RESULT = "PHASE_RESULT";
const PHASE_FALSE_START = "PHASE_FALSE_START";

let phase = PHASE_START;
let lightsOn = 0;
let phaseStartMs = 0;
let randomHoldMs = 0;
let reactionStartMs = 0;
let reactionTimeMs = 0;

let lastValidMs = 0;
let bestMs = 0;
let validCount = 0;
let falseStartCount = 0;
let historyMs = [];
let resultIsGreen = false;

const lightsEl = document.getElementById("lights");
const statusEl = document.getElementById("status");
const lastEl = document.getElementById("last");
const bestEl = document.getElementById("best");
const okEl = document.getElementById("ok");
const fsEl = document.getElementById("fs");
const barsEl = document.getElementById("bars");

for (let i = 0; i < LIGHT_COUNT; i++) {
  const d = document.createElement("div");
  d.className = "dot";
  lightsEl.appendChild(d);
}

function phaseLabel() {
  switch (phase) {
    case PHASE_START: return "Čeká na start";
    case PHASE_LIGHTING: return "Rozsvícení světel";
    case PHASE_WAIT_RANDOM: return "Čekání (náhodná prodleva)";
    case PHASE_MEASURE: return "Měření reakce!";
    case PHASE_RESULT: return "Výsledek";
    case PHASE_FALSE_START: return "Předčasná reakce";
    default: return "?";
  }
}

function randomHold() {
  return Math.floor(
    Math.random() * (RANDOM_DELAY_MAX_MS - RANDOM_DELAY_MIN_MS + 1)
  ) + RANDOM_DELAY_MIN_MS;
}

function recordValidResult(ms) {
  lastValidMs = ms;
  if (bestMs === 0 || ms < bestMs) {
    bestMs = ms;
  }
  validCount++;
  historyMs.push(ms);
  if (historyMs.length > HISTORY_SIZE) {
    historyMs.shift();
  }
}

function resetTrialToStart() {
  lightsOn = 0;
  randomHoldMs = 0;
  reactionTimeMs = 0;
  resultIsGreen = false;
  phase = PHASE_START;
  phaseStartMs = performance.now();
}

function startLighting() {
  lightsOn = 0;
  randomHoldMs = 0;
  reactionTimeMs = 0;
  resultIsGreen = false;
  phase = PHASE_LIGHTING;
  phaseStartMs = performance.now();
}

function onJerk() {
  const now = performance.now();

  if (phase === PHASE_START) {
    startLighting();
    return;
  }

  if (phase === PHASE_LIGHTING || phase === PHASE_WAIT_RANDOM) {
    phase = PHASE_FALSE_START;
    phaseStartMs = now;
    falseStartCount++;
    lightsOn = 0;
    resultIsGreen = false;
    return;
  }

  if (phase === PHASE_MEASURE) {
    reactionTimeMs = Math.round(now - reactionStartMs);
    recordValidResult(reactionTimeMs);
    phase = PHASE_RESULT;
    phaseStartMs = now;
    lightsOn = LIGHT_COUNT;
    resultIsGreen = true;
  }
}

function tick() {
  const now = performance.now();

  if (phase === PHASE_LIGHTING) {
    if (lightsOn === 0 || now - phaseStartMs >= LIGHT_STEP_MS) {
      if (lightsOn < LIGHT_COUNT) {
        lightsOn++;
        phaseStartMs = now;
      }
      if (lightsOn >= LIGHT_COUNT) {
        randomHoldMs = randomHold();
        phase = PHASE_WAIT_RANDOM;
        phaseStartMs = now;
      }
    }
  } else if (phase === PHASE_WAIT_RANDOM) {
    if (now - phaseStartMs >= randomHoldMs) {
      lightsOn = 0;
      resultIsGreen = false;
      reactionStartMs = performance.now();
      phase = PHASE_MEASURE;
    }
  } else if (phase === PHASE_RESULT || phase === PHASE_FALSE_START) {
    if (now - phaseStartMs >= RESULT_HOLD_MS) {
      resetTrialToStart();
    }
  }

  render();
  requestAnimationFrame(tick);
}

function render() {
  statusEl.textContent = phaseLabel();

  [...lightsEl.children].forEach((d, i) => {
    d.classList.toggle("on", !resultIsGreen && i < lightsOn);
    d.classList.toggle("green", resultIsGreen && i < lightsOn);
  });

  lastEl.textContent = lastValidMs ? lastValidMs + " ms" : "–";
  bestEl.textContent = bestMs ? bestMs + " ms" : "–";
  okEl.textContent = String(validCount);
  fsEl.textContent = String(falseStartCount);

  const max = Math.max(1000, ...historyMs);
  barsEl.innerHTML = "";
  historyMs.forEach((ms, i) => {
    const row = document.createElement("div");
    row.className = "bar-row";
    row.innerHTML =
      "<span>#" + (i + 1) + "</span>" +
      '<div class="bar" style="width:' + (ms / max * 100) + '%"></div>' +
      "<span>" + ms + " ms</span>";
    barsEl.appendChild(row);
  });
}

document.addEventListener("keydown", (e) => {
  if (e.key !== "ArrowUp") return;
  e.preventDefault();
  if (e.repeat) return;
  onJerk();
});

phaseStartMs = performance.now();
requestAnimationFrame(tick);
