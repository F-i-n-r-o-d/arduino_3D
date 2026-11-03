const LIGHT_COUNT = 5;
const LIGHT_STEP_MS = 500;
const RANDOM_DELAY_MIN_MS = 500;
const RANDOM_DELAY_MAX_MS = 5000;
const RESULT_HOLD_MS = 1500;
const GO_FLASH_MS = 220;
const HISTORY_SIZE = 8;

const PHASE_START = "PHASE_START";
const PHASE_GO_FLASH = "PHASE_GO_FLASH";
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
let lastWasNewBest = false;

const lightsEl = document.getElementById("lights");
const statusEl = document.getElementById("status");
const lastEl = document.getElementById("last");
const bestEl = document.getElementById("best");
const okEl = document.getElementById("ok");
const fsEl = document.getElementById("fs");
const barsEl = document.getElementById("bars");

for (let i = 0; i < LIGHT_COUNT; i++) {
  const d = document.createElement("div");
  d.className = "dot ready";
  lightsEl.appendChild(d);
}

function phaseLabel() {
  switch (phase) {
    case PHASE_START: return "Připraven — škubnutím START";
    case PHASE_GO_FLASH: return "Start! Připrav se";
    case PHASE_LIGHTING: return "Rozsvícení F1 světel";
    case PHASE_WAIT_RANDOM: return "Čekání (náhodná prodleva)";
    case PHASE_MEASURE: return "ZHASNUTO — teď měření!";
    case PHASE_RESULT: return lastWasNewBest ? "Nový rekord!" : "Výsledek";
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
  const isNewBest = bestMs === 0 || ms < bestMs;
  if (isNewBest) {
    bestMs = ms;
  }
  validCount++;
  historyMs.push(ms);
  if (historyMs.length > HISTORY_SIZE) {
    historyMs.shift();
  }
  return isNewBest;
}

function resetTrialToStart() {
  lightsOn = 0;
  randomHoldMs = 0;
  reactionTimeMs = 0;
  lastWasNewBest = false;
  phase = PHASE_START;
  phaseStartMs = performance.now();
}

function beginGoFlash() {
  lightsOn = 0;
  randomHoldMs = 0;
  reactionTimeMs = 0;
  lastWasNewBest = false;
  phase = PHASE_GO_FLASH;
  phaseStartMs = performance.now();
}

function startLighting() {
  lightsOn = 0;
  randomHoldMs = 0;
  reactionTimeMs = 0;
  lastWasNewBest = false;
  phase = PHASE_LIGHTING;
  phaseStartMs = performance.now();
}

function onJerk() {
  const now = performance.now();

  if (phase === PHASE_START) {
    beginGoFlash();
    return;
  }

  if (phase === PHASE_LIGHTING || phase === PHASE_WAIT_RANDOM) {
    phase = PHASE_FALSE_START;
    phaseStartMs = now;
    falseStartCount++;
    lightsOn = 0;
    lastWasNewBest = false;
    return;
  }

  if (phase === PHASE_MEASURE) {
    reactionTimeMs = Math.round(now - reactionStartMs);
    lastWasNewBest = recordValidResult(reactionTimeMs);
    phase = PHASE_RESULT;
    phaseStartMs = now;
    lightsOn = LIGHT_COUNT;
  }
}

function tick() {
  const now = performance.now();

  if (phase === PHASE_GO_FLASH) {
    if (now - phaseStartMs >= GO_FLASH_MS) {
      startLighting();
    }
  } else if (phase === PHASE_LIGHTING) {
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
      lastWasNewBest = false;
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
    d.className = "dot";
    if (phase === PHASE_START) {
      d.classList.add("ready");
    } else if (phase === PHASE_GO_FLASH) {
      d.classList.add("go");
    } else if (phase === PHASE_RESULT && i < lightsOn) {
      d.classList.add(lastWasNewBest ? "gold" : "green");
    } else if (i < lightsOn) {
      d.classList.add("on");
    }
  });

  lastEl.textContent = lastValidMs ? lastValidMs + " ms" : "–";
  bestEl.textContent = bestMs ? bestMs + " ms" : "–";
  okEl.textContent = String(validCount);
  fsEl.textContent = String(falseStartCount);

  const max = Math.max(1000, ...historyMs);
  barsEl.innerHTML = "";
  historyMs.forEach((ms, i) => {
    const isBest = bestMs > 0 && ms === bestMs;
    const row = document.createElement("div");
    row.className = "bar-row" + (isBest ? " best" : "");
    row.innerHTML =
      "<span>" + (isBest ? "nejlepší" : "#" + (i + 1)) + "</span>" +
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
