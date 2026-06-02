/*
 ╔══════════════════════════════════════════════════════════════╗
 ║         ESP32-S3 Web Oscilloscope + Function Generator       ║
 ║                                                              ║
 ║  Hardware:                                                   ║
 ║   - ESP32-S3 DevKit                                          ║
 ║   - MCP4725 DAC (I2C: SDA=GPIO8, SCL=GPIO9)                 ║
 ║   - LED (GPIO2 + 220Ω resistor to GND)                      ║
 ║   - Passive Buzzer (GPIO4)                                   ║
 ║   - ADC Input: GPIO1 (ADC1_CH0)                             ║
 ║                                                              ║
 ║  Features:                                                   ║
 ║   ✅ LED ON/OFF                                              ║
 ║   ✅ Oscilloscope START/STOP with Live Graph                 ║
 ║   ✅ Function Generator: Sine/Square/Triangle/Sawtooth       ║
 ║   ✅ Frequency & Amplitude Control                           ║
 ║   ✅ Measurements: Freq, Vpp, RMS                            ║
 ║   ✅ Musical Beep - follows selected waveform & frequency    ║
 ╚══════════════════════════════════════════════════════════════╝
*/

#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <math.h>

// ─── WiFi Config ─────────────────────────────────────────────
const char* WIFI_SSID     = "ssid";      // ← change this
const char* WIFI_PASSWORD = "password";  // ← change this

// ─── Pin Definitions ─────────────────────────────────────────
#define LED_PIN       2
#define BUZZER_PIN    4
#define ADC_PIN       1    // GPIO1 = ADC1_CH0
#define MCP4725_ADDR  0x60
#define SDA_PIN       9
#define SCL_PIN       8

// ─── Oscilloscope Settings ───────────────────────────────────
#define SAMPLE_COUNT  256
#define SAMPLE_RATE   10000  // Hz (10 kSPS)

// ─── Beep Settings ───────────────────────────────────────────
#define BEEP_DURATION_MS 250  // 250ms beep duration

// ─── State Variables ─────────────────────────────────────────
bool ledState       = false;
bool oscRunning     = false;
bool genRunning     = false;
int  waveType       = 0;   // 0=Sine 1=Square 2=Triangle 3=Sawtooth
float genFrequency  = 1000.0;  // Hz
float genAmplitude  = 1.0;     // 0.0 - 1.0

uint16_t adcSamples[SAMPLE_COUNT];
volatile bool samplesReady = false;

// Measurements
float measFreq = 0;
float measVpp  = 0;
float measRms  = 0;

WebServer server(80);

// ─── MCP4725 Write ───────────────────────────────────────────
void mcp4725Write(uint16_t value) {
  value = constrain(value, 0, 4095);
  Wire.beginTransmission(MCP4725_ADDR);
  Wire.write(0x40);
  Wire.write((value >> 4) & 0xFF);
  Wire.write((value << 4) & 0xFF);
  Wire.endTransmission();
}

// ─── Musical Beep - follows selected waveform & frequency ────
void musicalBeep() {
  // Use the current generator frequency for the beep (limited to audible range)
  float beepFreq = constrain(genFrequency, 100, 5000);
  
  Serial.printf("🎵 Beep: Wave=%d, Freq=%.1f Hz\n", waveType, beepFreq);
  
  // Calculate period in microseconds
  unsigned long periodMicros = 1000000UL / (unsigned long)beepFreq;
  unsigned long halfPeriodMicros = periodMicros / 2;
  
  // Calculate number of cycles to play
  int cycles = (BEEP_DURATION_MS * beepFreq) / 1000;
  if (cycles < 5) cycles = 5;
  
  for (int cycle = 0; cycle < cycles; cycle++) {
    // Get normalized position within cycle (0 to 1)
    float t = (float)(cycle % 2) / 2.0f;
    
    // For sine wave, we need more resolution - use micros for smooth sine
    if (waveType == 0) {
      // Sine wave - smooth pitch
      unsigned long startMicros = micros();
      while (micros() - startMicros < periodMicros) {
        float phase = (float)(micros() - startMicros) / periodMicros;
        float val = 0.5f + 0.5f * sinf(2.0f * M_PI * phase);
        int duty = (int)(val * 255);
        analogWrite(BUZZER_PIN, duty);
        delayMicroseconds(20); // Small delay for stability
      }
    } 
    else if (waveType == 1) {
      // Square wave - classic beep
      digitalWrite(BUZZER_PIN, HIGH);
      delayMicroseconds(halfPeriodMicros);
      digitalWrite(BUZZER_PIN, LOW);
      delayMicroseconds(halfPeriodMicros);
    }
    else if (waveType == 2) {
      // Triangle wave - soft rising/falling
      unsigned long startMicros = micros();
      while (micros() - startMicros < periodMicros) {
        float phase = (float)(micros() - startMicros) / periodMicros;
        float val = (phase < 0.5f) ? (2.0f * phase) : (2.0f - 2.0f * phase);
        int duty = (int)(val * 255);
        analogWrite(BUZZER_PIN, duty);
        delayMicroseconds(20);
      }
    }
    else if (waveType == 3) {
      // Sawtooth wave - aggressive rising
      unsigned long startMicros = micros();
      while (micros() - startMicros < periodMicros) {
        float phase = (float)(micros() - startMicros) / periodMicros;
        float val = phase;  // Rising sawtooth
        int duty = (int)(val * 255);
        analogWrite(BUZZER_PIN, duty);
        delayMicroseconds(20);
      }
    }
  }
  
  // Turn off buzzer
  analogWrite(BUZZER_PIN, 0);
  digitalWrite(BUZZER_PIN, LOW);
}

// ─── Alternative: Sweep beep that follows generator freq ─────
void musicalBeepSweep() {
  float startFreq = constrain(genFrequency * 0.7f, 100, 5000);
  float endFreq = constrain(genFrequency * 1.3f, 100, 5000);
  
  Serial.printf("🎵 Sweep Beep: %.0fHz → %.0fHz\n", startFreq, endFreq);
  
  int steps = 80;
  unsigned long stepDuration = (BEEP_DURATION_MS * 1000) / steps;
  
  for (int i = 0; i <= steps; i++) {
    float t = (float)i / steps;
    // Exponential sweep sounds more musical
    float freq = startFreq * pow(endFreq / startFreq, t);
    
    unsigned long halfPeriod = 500000UL / (unsigned long)freq;
    unsigned long startTime = micros();
    
    // Play this frequency for stepDuration
    while (micros() - startTime < stepDuration) {
      digitalWrite(BUZZER_PIN, HIGH);
      delayMicroseconds(halfPeriod);
      digitalWrite(BUZZER_PIN, LOW);
      delayMicroseconds(halfPeriod);
    }
  }
  
  digitalWrite(BUZZER_PIN, LOW);
}

// ─── Function Generator Task ─────────────────────────────────
void generatorTask(void* param) {
  uint32_t phase = 0;
  while (true) {
    if (genRunning) {
      uint32_t phaseStep = (uint32_t)((4294967296.0 * genFrequency) / 100000.0);
      float t = (float)phase / 4294967296.0;
      float val = 0;

      switch (waveType) {
        case 0: // Sine
          val = 0.5f + 0.5f * sinf(2.0f * M_PI * t);
          break;
        case 1: // Square
          val = (t < 0.5f) ? 1.0f : 0.0f;
          break;
        case 2: // Triangle
          val = (t < 0.5f) ? (2.0f * t) : (2.0f - 2.0f * t);
          break;
        case 3: // Sawtooth
          val = t;
          break;
      }

      uint16_t dacVal = (uint16_t)(val * genAmplitude * 4095);
      mcp4725Write(dacVal);
      phase += phaseStep;
      delayMicroseconds(10);
    } else {
      mcp4725Write(2048);
      delay(10);
    }
    vTaskDelay(0);
  }
}

// ─── Sample ADC ──────────────────────────────────────────────
void takeSamples() {
  uint32_t interval = 1000000 / SAMPLE_RATE;
  for (int i = 0; i < SAMPLE_COUNT; i++) {
    adcSamples[i] = analogRead(ADC_PIN);
    delayMicroseconds(interval);
  }
  samplesReady = true;
  calculateMeasurements();
}

void calculateMeasurements() {
  uint16_t minVal = 4095, maxVal = 0;
  float sumSq = 0;
  for (int i = 0; i < SAMPLE_COUNT; i++) {
    if (adcSamples[i] < minVal) minVal = adcSamples[i];
    if (adcSamples[i] > maxVal) maxVal = adcSamples[i];
    float v = adcSamples[i] * 3.3f / 4095.0f;
    sumSq += v * v;
  }
  measVpp = (maxVal - minVal) * 3.3f / 4095.0f;
  measRms = sqrtf(sumSq / SAMPLE_COUNT);

  float mean = 0;
  for (int i = 0; i < SAMPLE_COUNT; i++) mean += adcSamples[i];
  mean /= SAMPLE_COUNT;

  int crossings = 0;
  for (int i = 1; i < SAMPLE_COUNT; i++) {
    if (adcSamples[i-1] < mean && adcSamples[i] >= mean) crossings++;
  }
  float duration = (float)SAMPLE_COUNT / SAMPLE_RATE;
  measFreq = (crossings > 0) ? (crossings / (2.0f * duration)) : 0;
}

// ─── Web Page HTML ───────────────────────────────────────────
const char HTML_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="ar" dir="rtl">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>ESP32 Oscilloscope</title>
<link href="https://fonts.googleapis.com/css2?family=Share+Tech+Mono&family=Tajawal:wght@300;400;700&display=swap" rel="stylesheet">
<style>
  :root {
    --bg:       #0a0e14;
    --panel:    #0f1520;
    --border:   #1e3a5f;
    --accent:   #00d4ff;
    --accent2:  #00ff88;
    --accent3:  #ff6b35;
    --warn:     #ffcc00;
    --text:     #c8d8e8;
    --dim:      #4a6080;
  }
  * { margin:0; padding:0; box-sizing:border-box; }
  body {
    background: var(--bg);
    color: var(--text);
    font-family: 'Tajawal', sans-serif;
    min-height: 100vh;
  }
  header {
    display: flex;
    align-items: center;
    justify-content: space-between;
    padding: 16px 28px;
    border-bottom: 1px solid var(--border);
    background: rgba(15,21,32,0.9);
    backdrop-filter: blur(10px);
  }
  .logo {
    font-family: 'Share Tech Mono', monospace;
    font-size: 1.3rem;
    color: var(--accent);
  }
  .logo span { color: var(--accent2); }
  .status-bar {
    display: flex; gap: 16px;
    font-family: 'Share Tech Mono', monospace;
    font-size: 0.75rem;
  }
  .status-dot {
    width: 8px; height: 8px; border-radius: 50%;
    background: var(--dim); display: inline-block;
    margin-left: 6px;
  }
  .status-dot.active {
    background: var(--accent2);
    box-shadow: 0 0 8px var(--accent2);
    animation: pulse 1.5s infinite;
  }
  @keyframes pulse {
    0%,100% { opacity:1; } 50% { opacity:0.5; }
  }
  .main { padding: 20px 24px; display: flex; flex-direction: column; gap: 16px; }
  .panel {
    background: var(--panel);
    border: 1px solid var(--border);
    border-radius: 12px;
    padding: 16px 20px;
  }
  .panel-title {
    font-family: 'Share Tech Mono', monospace;
    font-size: 0.7rem;
    color: var(--dim);
    text-transform: uppercase;
    margin-bottom: 14px;
  }
  .scope-wrapper {
    position: relative;
    background: #050a0f;
    border-radius: 8px;
    border: 1px solid var(--border);
  }
  canvas#scope {
    display: block;
    width: 100%;
    height: 220px;
    background: #050a0f;
  }
  .measurements {
    display: grid;
    grid-template-columns: repeat(3, 1fr);
    gap: 10px;
    margin-top: 12px;
  }
  .meas-card {
    background: rgba(0,0,0,0.3);
    border: 1px solid var(--border);
    border-radius: 8px;
    padding: 10px 12px;
    text-align: center;
  }
  .meas-label {
    font-family: 'Share Tech Mono', monospace;
    font-size: 0.6rem;
    color: var(--dim);
  }
  .meas-value {
    font-family: 'Share Tech Mono', monospace;
    font-size: 1.1rem;
    font-weight: bold;
  }
  .meas-value.freq { color: var(--accent); }
  .meas-value.vpp  { color: var(--accent2); }
  .meas-value.rms  { color: var(--accent3); }
  .controls-grid {
    display: grid;
    grid-template-columns: 1fr 1fr;
    gap: 12px;
  }
  .btn {
    display: flex; align-items: center; justify-content: center; gap: 8px;
    padding: 12px 18px;
    border: 1px solid var(--border);
    border-radius: 8px;
    background: rgba(0,0,0,0.3);
    color: var(--text);
    font-family: 'Share Tech Mono', monospace;
    font-size: 0.85rem;
    cursor: pointer;
    transition: all 0.2s;
    width: 100%;
  }
  .btn:hover { border-color: var(--accent); color: var(--accent); }
  .btn.active-green {
    background: rgba(0,255,136,0.1);
    border-color: var(--accent2);
    color: var(--accent2);
  }
  .btn.active-blue {
    background: rgba(0,212,255,0.1);
    border-color: var(--accent);
    color: var(--accent);
  }
  .btn.active-orange {
    background: rgba(255,107,53,0.1);
    border-color: var(--accent3);
    color: var(--accent3);
  }
  .wave-grid {
    display: grid;
    grid-template-columns: repeat(4, 1fr);
    gap: 8px;
    margin-top: 4px;
  }
  .wave-btn {
    padding: 10px 4px;
    border: 1px solid var(--border);
    border-radius: 8px;
    background: rgba(0,0,0,0.3);
    color: var(--dim);
    cursor: pointer;
    font-family: 'Share Tech Mono', monospace;
    font-size: 0.7rem;
    text-align: center;
    transition: all 0.2s;
  }
  .wave-btn svg { width: 36px; height: 20px; display: block; margin: 0 auto 5px; }
  .wave-btn:hover { border-color: var(--accent); color: var(--accent); }
  .wave-btn.selected {
    border-color: var(--warn);
    color: var(--warn);
    background: rgba(255,204,0,0.08);
  }
  .slider-row {
    display: flex; align-items: center; gap: 12px; margin-top: 8px;
  }
  .slider-label {
    font-family: 'Share Tech Mono', monospace;
    font-size: 0.7rem;
    color: var(--dim);
    min-width: 60px;
  }
  input[type=range] {
    flex: 1;
    -webkit-appearance: none;
    height: 4px;
    border-radius: 2px;
    background: var(--border);
  }
  input[type=range]::-webkit-slider-thumb {
    -webkit-appearance: none;
    width: 14px; height: 14px;
    border-radius: 50%;
    background: var(--accent);
    cursor: pointer;
  }
  .slider-val {
    font-family: 'Share Tech Mono', monospace;
    font-size: 0.8rem;
    color: var(--accent);
    min-width: 65px;
  }
  .led-visual {
    width: 28px; height: 28px; border-radius: 50%;
    background: var(--dim);
    margin: 0 auto 10px;
    transition: all 0.3s;
  }
  .led-visual.on {
    background: #00ff88;
    box-shadow: 0 0 20px rgba(0,255,136,0.8);
  }
  .beep-preview {
    margin-top: 8px;
    font-size: 0.7rem;
    color: var(--dim);
    text-align: center;
  }
  #toast {
    position: fixed; bottom: 20px; left: 50%; transform: translateX(-50%);
    background: rgba(0,212,255,0.15);
    border: 1px solid var(--accent);
    color: var(--accent);
    padding: 8px 20px;
    border-radius: 20px;
    font-family: 'Share Tech Mono', monospace;
    font-size: 0.8rem;
    opacity: 0; transition: opacity 0.3s;
    pointer-events: none;
    z-index: 999;
  }
  #toast.show { opacity: 1; }
</style>
</head>
<body>
<header>
  <div class="logo">ESP32<span>-S3</span> // OSCILLOSCOPE</div>
  <div class="status-bar">
    <span><span class="status-dot" id="osc-dot"></span>OSC</span>
    <span><span class="status-dot" id="gen-dot"></span>GEN</span>
    <span><span class="status-dot active" id="wifi-dot"></span>WIFI</span>
  </div>
</header>
<div class="main">
  <div class="panel">
    <div class="panel-title">▸ OSCILLOSCOPE</div>
    <div class="scope-wrapper">
      <canvas id="scope" width="800" height="220"></canvas>
    </div>
    <div class="measurements">
      <div class="meas-card"><div class="meas-label">FREQ</div><div class="meas-value freq" id="m-freq">— Hz</div></div>
      <div class="meas-card"><div class="meas-label">Vpp</div><div class="meas-value vpp" id="m-vpp">— V</div></div>
      <div class="meas-card"><div class="meas-label">RMS</div><div class="meas-value rms" id="m-rms">— V</div></div>
    </div>
    <div style="margin-top:12px"><button class="btn" id="osc-btn" onclick="toggleOsc()">▶ START OSCILLOSCOPE</button></div>
  </div>
  <div class="panel">
    <div class="panel-title">▸ FUNCTION GENERATOR</div>
    <div class="wave-grid">
      <div class="wave-btn selected" id="wave-0" onclick="setWave(0)"><svg viewBox="0 0 36 20"><path d="M2 10 Q9 2 18 10 Q27 18 36 10" stroke="currentColor" fill="none" stroke-width="1.5"/></svg>SINE</div>
      <div class="wave-btn" id="wave-1" onclick="setWave(1)"><svg viewBox="0 0 36 20"><path d="M2 16 L2 4 L18 4 L18 16 L36 16 L36 4" stroke="currentColor" fill="none" stroke-width="1.5"/></svg>SQR</div>
      <div class="wave-btn" id="wave-2" onclick="setWave(2)"><svg viewBox="0 0 36 20"><path d="M2 16 L10 4 L18 16 L26 4 L36 16" stroke="currentColor" fill="none" stroke-width="1.5"/></svg>TRI</div>
      <div class="wave-btn" id="wave-3" onclick="setWave(3)"><svg viewBox="0 0 36 20"><path d="M2 4 L16 16 L16 4 L30 16 L30 4" stroke="currentColor" fill="none" stroke-width="1.5"/></svg>SAW</div>
    </div>
    <div class="slider-row"><span class="slider-label">FREQ</span><input type="range" id="freq-slider" min="10" max="10000" value="1000" oninput="updateFreq(this.value)"><span class="slider-val" id="freq-val">1000 Hz</span></div>
    <div class="slider-row"><span class="slider-label">AMP</span><input type="range" id="amp-slider" min="0" max="100" value="100" oninput="updateAmp(this.value)"><span class="slider-val" id="amp-val">100 %</span></div>
    <div style="margin-top:12px"><button class="btn" id="gen-btn" onclick="toggleGen()">▶ START GENERATOR</button></div>
    <div class="beep-preview">🎵 Musical Beep follows selected waveform + frequency</div>
  </div>
  <div class="panel">
    <div class="panel-title">▸ CONTROLS</div>
    <div class="controls-grid">
      <div style="text-align:center"><div class="led-visual" id="led-vis"></div><button class="btn" id="led-btn" onclick="toggleLed()">💡 LED OFF</button></div>
      <div style="display:flex; align-items:center"><button class="btn" onclick="beep()" style="height:100%">🎵 MUSICAL BEEP</button></div>
    </div>
  </div>
</div>
<div id="toast"></div>
<script>
let oscRunning = false, genRunning = false, ledOn = false, oscInterval = null, currentWave = 0;
const canvas = document.getElementById('scope'), ctx = canvas.getContext('2d');
function resizeCanvas() {
  const wrapper = canvas.parentElement;
  canvas.width = wrapper.clientWidth;
  canvas.height = 220;
  canvas.style.width = '100%';
  drawIdle();
}
window.addEventListener('resize', resizeCanvas);
setTimeout(resizeCanvas, 100);
function drawGrid(w, h) {
  ctx.strokeStyle = '#0d2035';
  ctx.lineWidth = 1;
  for(let i=0;i<=10;i++){ ctx.beginPath(); ctx.moveTo(i*w/10,0); ctx.lineTo(i*w/10,h); ctx.stroke(); }
  for(let j=0;j<=6;j++){ ctx.beginPath(); ctx.moveTo(0,j*h/6); ctx.lineTo(w,j*h/6); ctx.stroke(); }
  ctx.strokeStyle = '#112233';
  ctx.beginPath(); ctx.moveTo(0,h/2); ctx.lineTo(w,h/2); ctx.stroke();
  ctx.beginPath(); ctx.moveTo(w/2,0); ctx.lineTo(w/2,h); ctx.stroke();
}
function drawIdle() {
  const w = canvas.width, h = canvas.height;
  ctx.fillStyle = '#050a0f'; ctx.fillRect(0,0,w,h);
  drawGrid(w,h);
  ctx.fillStyle = 'rgba(0,212,255,0.15)';
  ctx.font = "14px 'Share Tech Mono', monospace";
  ctx.textAlign = 'center';
  ctx.fillText('[ PRESS START TO ACQUIRE ]', w/2, h/2 + 5);
}
function drawWaveform(samples) {
  const w = canvas.width, h = canvas.height;
  ctx.fillStyle = '#050a0f'; ctx.fillRect(0,0,w,h);
  drawGrid(w,h);
  if(!samples) return;
  ctx.shadowColor = 'rgba(0,212,255,0.6)'; ctx.shadowBlur = 6;
  ctx.strokeStyle = '#00d4ff'; ctx.lineWidth = 1.5; ctx.beginPath();
  for(let i=0;i<samples.length;i++) {
    const x = (i/samples.length)*w, y = h - (samples[i]/4095)*h;
    i===0 ? ctx.moveTo(x,y) : ctx.lineTo(x,y);
  }
  ctx.stroke(); ctx.shadowBlur = 0;
}
async function api(endpoint) {
  try { const res = await fetch(endpoint); return await res.json(); } catch(e) { toast('Connection error'); return null; }
}
function toast(msg) { const t = document.getElementById('toast'); t.textContent = msg; t.classList.add('show'); setTimeout(()=>t.classList.remove('show'),2000); }
async function toggleOsc() {
  oscRunning = !oscRunning;
  const btn = document.getElementById('osc-btn'), dot = document.getElementById('osc-dot');
  await api('/osc/'+(oscRunning?'start':'stop'));
  if(oscRunning) {
    btn.textContent = '⏹ STOP OSCILLOSCOPE'; btn.className = 'btn active-blue';
    dot.classList.add('active'); oscInterval = setInterval(fetchSamples,200);
    toast('Oscilloscope started');
  } else {
    btn.textContent = '▶ START OSCILLOSCOPE'; btn.className = 'btn';
    dot.classList.remove('active'); clearInterval(oscInterval); drawIdle();
    toast('Oscilloscope stopped');
  }
}
async function fetchSamples() {
  const data = await api('/osc/data');
  if(!data) return;
  drawWaveform(data.samples);
  const f = data.freq;
  document.getElementById('m-freq').textContent = f>=1000?(f/1000).toFixed(2)+' kHz':f.toFixed(1)+' Hz';
  document.getElementById('m-vpp').textContent = data.vpp.toFixed(3)+' V';
  document.getElementById('m-rms').textContent = data.rms.toFixed(3)+' V';
}
async function toggleGen() {
  genRunning = !genRunning;
  const btn = document.getElementById('gen-btn'), dot = document.getElementById('gen-dot');
  await api('/gen/'+(genRunning?'start':'stop'));
  if(genRunning) {
    btn.textContent = '⏹ STOP GENERATOR'; btn.className = 'btn active-orange';
    dot.classList.add('active'); toast('Generator started');
  } else {
    btn.textContent = '▶ START GENERATOR'; btn.className = 'btn';
    dot.classList.remove('active'); toast('Generator stopped');
  }
}
function setWave(type) {
  currentWave = type;
  document.querySelectorAll('.wave-btn').forEach(b=>b.classList.remove('selected'));
  document.getElementById('wave-'+type).classList.add('selected');
  api('/gen/wave?value='+type);
}
function updateFreq(val) {
  document.getElementById('freq-val').textContent = val>=1000?(val/1000).toFixed(1)+' kHz':val+' Hz';
  api('/gen/freq?value='+val);
}
function updateAmp(val) {
  document.getElementById('amp-val').textContent = val+' %';
  api('/gen/amp?value='+val);
}
async function toggleLed() {
  ledOn = !ledOn;
  const btn = document.getElementById('led-btn'), vis = document.getElementById('led-vis');
  await api('/led/'+(ledOn?'on':'off'));
  if(ledOn) { btn.textContent = '💡 LED ON'; btn.className = 'btn active-green'; vis.classList.add('on'); }
  else { btn.textContent = '💡 LED OFF'; btn.className = 'btn'; vis.classList.remove('on'); }
}
async function beep() {
  await api('/buzzer/beep');
  const waveNames = ['SINE', 'SQUARE', 'TRIANGLE', 'SAWTOOTH'];
  toast(`🎵 ${waveNames[currentWave]} beep @ ${document.getElementById('freq-val').textContent}`);
}
drawIdle();
</script>
</body>
</html>
)rawhtml";

// ─── HTTP Handlers ───────────────────────────────────────────
void handleRoot() {
  server.send_P(200, "text/html", HTML_PAGE);
}

void handleOscStart() {
  oscRunning = true;
  server.send(200, "application/json", "{\"status\":\"started\"}");
}

void handleOscStop() {
  oscRunning = false;
  server.send(200, "application/json", "{\"status\":\"stopped\"}");
}

void handleOscData() {
  if (oscRunning) takeSamples();
  String json = "{\"samples\":[";
  for (int i = 0; i < SAMPLE_COUNT; i++) {
    json += adcSamples[i];
    if (i < SAMPLE_COUNT - 1) json += ",";
  }
  json += "],\"freq\":" + String(measFreq) + ",\"vpp\":" + String(measVpp) + ",\"rms\":" + String(measRms) + "}";
  server.send(200, "application/json", json);
}

void handleGenStart() {
  genRunning = true;
  server.send(200, "application/json", "{\"status\":\"started\"}");
}

void handleGenStop() {
  genRunning = false;
  server.send(200, "application/json", "{\"status\":\"stopped\"}");
}

void handleWave() {
  waveType = server.arg("value").toInt();
  server.send(200, "application/json", "{\"wave\":" + String(waveType) + "}");
}

void handleFreq() {
  genFrequency = server.arg("value").toFloat();
  server.send(200, "application/json", "{\"freq\":" + String(genFrequency) + "}");
}

void handleAmp() {
  genAmplitude = server.arg("value").toFloat() / 100.0f;
  server.send(200, "application/json", "{\"amp\":" + String(genAmplitude) + "}");
}

void handleLedOn()  { ledState = true;  digitalWrite(LED_PIN, HIGH); server.send(200, "application/json", "{\"led\":1}"); }
void handleLedOff() { ledState = false; digitalWrite(LED_PIN, LOW);  server.send(200, "application/json", "{\"led\":0}"); }

void handleBuzzer() {
  musicalBeep();  // Now plays the selected waveform at the selected frequency!
  server.send(200, "application/json", "{\"beep\":1}");
}

// ─── Setup ───────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(ADC_PIN, INPUT);
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);
  
  Wire.begin(SDA_PIN, SCL_PIN);
  mcp4725Write(2048);
  
  Serial.print("Connecting to WiFi");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  int tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries < 30) {
    delay(500); Serial.print("."); tries++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ Connected! IP: " + WiFi.localIP().toString());
  } else {
    Serial.println("\n⚠️ WiFi failed — starting AP mode");
    WiFi.softAP("ESP32-Oscilloscope", "12345678");
    Serial.println("AP IP: " + WiFi.softAPIP().toString());
  }
  
  server.on("/", handleRoot);
  server.on("/osc/start", handleOscStart);
  server.on("/osc/stop", handleOscStop);
  server.on("/osc/data", handleOscData);
  server.on("/gen/start", handleGenStart);
  server.on("/gen/stop", handleGenStop);
  server.on("/gen/wave", handleWave);
  server.on("/gen/freq", handleFreq);
  server.on("/gen/amp", handleAmp);
  server.on("/led/on", handleLedOn);
  server.on("/led/off", handleLedOff);
  server.on("/buzzer/beep", handleBuzzer);
  
  server.begin();
  Serial.println("🌐 Web server started");
  
  xTaskCreatePinnedToCore(generatorTask, "GenTask", 4096, NULL, 1, NULL, 0);
}

void loop() {
  server.handleClient();
}
