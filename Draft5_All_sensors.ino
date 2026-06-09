// integrate Age of rock
#define USE_WIFI_NINA         false
#define USE_WIFI101           true
#include <WiFiWebServer.h>
#include <Wire.h>
#include "DFRobot_BMM150.h"

//defining pin use
#define IR_PIN  2       // IR pulse measurement pin

//defining parameters of IR detection
const int IR_MEASURE_WINDOW_MS =  3000;    // IR Measurement duration (ms)
volatile uint32_t pulseCount = 0;  // Incremented by ISR
uint32_t windowStart;   // Timestamp of window start
void onPulseDetected() {
  pulseCount++;
}
int IR_emission_category = -1; //to inform us whether the rock is emitting IR at 312/s or 547/s
bool measuring_IR = false;

int US_pin = A0;

const int AGE_MEASURE_WINDOW_MS = 200;

#ifndef FPSTR
#define FPSTR(pstr) (reinterpret_cast<const __FlashStringHelper *>(pstr))
#endif


const char ssid[] = "EEERover";
const char pass[] = "exhibition";
const int groupNumber = 3; // Set your group number to make the IP address constant - only do this on the EEERover network

// Motor pins
const int LEFT_PWM  = 6;
const int LEFT_DIR  = 8;
const int RIGHT_PWM = 3;
const int RIGHT_DIR = 4;

// Speeds
const int drive_speed = 255;
const int turn_speed  = 180;

unsigned long LastCommandTime = 0; //Timestamp of the last command from when the board started up
bool MotorState = false;

DFRobot_BMM150_I2C bmm150(&Wire, I2C_ADDRESS_4);


void setMotor(int DirPin, int PWMPin, int speed) {
  if (speed > 0) {
    analogWrite(PWMPin, speed);
    digitalWrite(DirPin, 1);
  } else if (speed < 0) {
    int abs_speed = -speed;
    analogWrite(PWMPin, abs_speed);
    digitalWrite(DirPin, 0);
  } else {
    analogWrite(PWMPin, 0);
  }
}

// Your movement functions
void DriveForward() {
  setMotor(LEFT_DIR,  LEFT_PWM,  drive_speed);
  setMotor(RIGHT_DIR, RIGHT_PWM, drive_speed);
}

void DriveBackward() {
  setMotor(LEFT_DIR,  LEFT_PWM,  -drive_speed);
  setMotor(RIGHT_DIR, RIGHT_PWM, -drive_speed);
}

void TurnLeft() {
  setMotor(LEFT_DIR,  LEFT_PWM,  turn_speed);
  setMotor(RIGHT_DIR, RIGHT_PWM, -turn_speed);
}

void TurnRight() {
  setMotor(LEFT_DIR,  LEFT_PWM,  -turn_speed);
  setMotor(RIGHT_DIR, RIGHT_PWM, turn_speed);
}

void DriveStop() {
  setMotor(LEFT_DIR,  LEFT_PWM,  0);
  setMotor(RIGHT_DIR, RIGHT_PWM, 0);
}
//Webpage to return when root is requested

char readMagnetic() {
  sBmm150MagData_t magData = bmm150.getGeomagneticData();
  
  if(magData.z < 0){
    return 'U';
  }
  else{
    return 'D';
  }
}

bool readUltrasound(){
  int US_value = analogRead(US_pin);

  if(US_value > 512){
    return true;
  }
  else{
    return false;
  }
}

String readAge(){
  unsigned long startTime = millis();
  bool foundHash = false;
  while(millis() - startTime < AGE_MEASURE_WINDOW_MS){
    if((Serial1.available()) && (Serial1.read() == '#')){
      foundHash = true;
      break;
    }
  }

  if(foundHash == false){
    return "error";
  }

  String result = "#";
  for(int i = 0; i < 3; i++){
    startTime = millis();
    while(!Serial1.available()){
      if(millis() - startTime > AGE_MEASURE_WINDOW_MS){
        return "error";
      }
    }
    char nextByte = (char)Serial1.read(); // treat the number as a character instead
    result += nextByte; // append to string
  }
  return result;
}


const char page_head[] PROGMEM =
"* { margin:0; padding:0; box-sizing:border-box; }

  body {
    font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', sans-serif;
    background: #f5f5f5;
    color: #111;
    height: 100vh;
    display: flex;
    flex-direction: column;
  }

  header {
    background: #fff;
    border-bottom: 1px solid #e0e0e0;
    padding: 12px 24px;
    display: flex;
    align-items: center;
    justify-content: space-between;
  }

  header h1 { font-size: 15px; font-weight: 600; }

  .conn {
    display: flex; align-items: center; gap: 6px;
    font-size: 13px; color: #666;
  }
  .dot {
    width: 8px; height: 8px; border-radius: 50%;
    background: #22c55e;
  }
  .dot.off { background: #ef4444; }

  .layout {
    flex: 1;
    display: grid;
    grid-template-columns: 1fr 320px;
    gap: 0;
    overflow: hidden;
  }

  /* ── LEFT ── */
  .left {
    padding: 32px;
    display: flex;
    flex-direction: column;
    align-items: center;
    justify-content: center;
    gap: 28px;
    border-right: 1px solid #e0e0e0;
    background: #fff;
  }

  .section-label {
    font-size: 11px;
    font-weight: 600;
    letter-spacing: 1px;
    text-transform: uppercase;
    color: #999;
    align-self: flex-start;
  }

  /* dpad */
  .dpad {
    display: grid;
    grid-template-columns: repeat(3, 72px);
    grid-template-rows: repeat(3, 72px);
    gap: 6px;
  }

  .btn {
    background: #f5f5f5;
    border: 1px solid #ddd;
    border-radius: 8px;
    font-size: 13px;
    font-weight: 600;
    color: #333;
    cursor: pointer;
    display: flex;
    flex-direction: column;
    align-items: center;
    justify-content: center;
    gap: 3px;
    transition: background .1s, border-color .1s, transform .07s;
    user-select: none;
  }
  .btn .ico { font-size: 18px; }
  .btn .lbl { font-size: 9px; color: #999; letter-spacing: 1px; }

  .btn:active, .btn.on {
    background: #111;
    border-color: #111;
    color: #fff;
    transform: scale(.94);
  }
  .btn:active .lbl, .btn.on .lbl { color: #aaa; }

  .btn-fwd { grid-column:2; grid-row:1; }
  .btn-l   { grid-column:1; grid-row:2; }
  .btn-stp {
    grid-column:2; grid-row:2;
    border-color: #fca5a5;
    color: #ef4444;
  }
  .btn-stp:active, .btn-stp.on {
    background: #ef4444;
    border-color: #ef4444;
    color: #fff;
  }
  .btn-r   { grid-column:3; grid-row:2; }
  .btn-bk  { grid-column:2; grid-row:3; }

  /* speed */
  .speed-row { width: 240px; display: flex; flex-direction: column; gap: 8px; }
  .speed-top { display: flex; justify-content: space-between; font-size: 12px; color: #666; }
  input[type=range] {
    width: 100%; appearance: none; height: 4px;
    background: linear-gradient(to right, #111 var(--p,60%), #e0e0e0 var(--p,60%));
    border-radius: 2px; outline: none; cursor: pointer;
  }
  input[type=range]::-webkit-slider-thumb {
    appearance: none; width: 14px; height: 14px;
    border-radius: 50%; background: #111;
  }

  /* kbd hint */
  .kbd-hint { font-size: 12px; color: #999; }
  kbd {
    display: inline-flex; align-items: center; justify-content: center;
    width: 20px; height: 20px;
    border: 1px solid #ddd; border-radius: 4px;
    background: #f5f5f5; font-size: 10px; font-family: inherit;
    margin: 0 1px;
  }

  /* ── RIGHT ── */
  .right {
    display: flex; flex-direction: column;
    overflow-y: auto;
    background: #fafafa;
  }

  .right-inner { padding: 20px; display: flex; flex-direction: column; gap: 16px; }

  /* rock card */
  .rock-card {
    background: #fff;
    border: 1px solid #e0e0e0;
    border-radius: 10px;
    overflow: hidden;
  }

  .rock-top {
    display: flex; justify-content: space-between; align-items: flex-start;
    padding: 14px 16px;
    border-bottom: 1px solid #f0f0f0;
  }

  .rock-type {
    font-size: 20px; font-weight: 700; color: #111;
  }
  .rock-type.unknown { color: #bbb; font-weight: 400; }
  .rock-type.Basaltoid { color: #d97706; }
  .rock-type.Gravion   { color: #16a34a; }
  .rock-type.Regolix   { color: #7c3aed; }
  .rock-type.Lunarite  { color: #2563eb; }

  .rock-age { font-size: 12px; color: #999; margin-top: 4px; }

  /* sensor grid */
  .sgrid {
    display: grid; grid-template-columns: 1fr 1fr;
    gap: 1px; background: #f0f0f0;
  }
  .scell {
    background: #fff; padding: 12px 14px;
    display: flex; flex-direction: column; gap: 3px;
  }
  .sname { font-size: 10px; font-weight: 600; color: #999; text-transform: uppercase; letter-spacing: .5px; }
  .sval  { font-size: 18px; font-weight: 700; color: #bbb; }
  .sval.hi { color: #111; }
  .sunit { font-size: 10px; color: #bbb; }

  /* ir bar */
  .ir-wrap { padding: 12px 14px; }
  .ir-top { display: flex; justify-content: space-between; font-size: 11px; color: #999; margin-bottom: 6px; }
  .ir-track { height: 5px; background: #f0f0f0; border-radius: 3px; position: relative; overflow: hidden; }
  .ir-fill { height: 100%; background: #111; border-radius: 3px; width: 0; transition: width .4s; }
  .ir-thresh { position: absolute; top:0; bottom:0; width: 2px; background: #ef4444; left: 71.6%; }

  /* scan btn */
  .scan-btn {
    width: 100%; padding: 12px;
    background: #111; border: none; border-radius: 8px;
    color: #fff; font-size: 14px; font-weight: 600;
    cursor: pointer; transition: background .15s;
  }
  .scan-btn:hover { background: #333; }
  .scan-btn:disabled { background: #999; cursor: not-allowed; }
  .scan-btn.scanning { background: #d97706; }

  .prog-wrap { height: 3px; background: #e0e0e0; border-radius: 2px; overflow: hidden; }
  .prog-fill { height: 100%; background: #d97706; width: 0; transition: width .3s linear; border-radius: 2px; }

  /* log */
  .log-card {
    background: #fff; border: 1px solid #e0e0e0;
    border-radius: 10px; overflow: hidden;
  }
  .log-header {
    padding: 10px 14px;
    border-bottom: 1px solid #f0f0f0;
    font-size: 12px; font-weight: 600; color: #666;
    display: flex; justify-content: space-between;
  }
  .log-entries { max-height: 200px; overflow-y: auto; }
  .log-row {
    display: grid; grid-template-columns: 28px 1fr 70px 52px;
    gap: 6px; align-items: center;
    padding: 8px 14px;
    border-bottom: 1px solid #f5f5f5;
    font-size: 12px;
    animation: fadein .15s ease;
  }
  @keyframes fadein { from{opacity:0} to{opacity:1} }
  .log-row:last-child { border-bottom: none; }
  .log-n { color: #bbb; font-size: 11px; }
  .log-t { font-weight: 700; }
  .log-t.Basaltoid { color: #d97706; }
  .log-t.Gravion   { color: #16a34a; }
  .log-t.Regolix   { color: #7c3aed; }
  .log-t.Lunarite  { color: #2563eb; }
  .log-t.UNKNOWN   { color: #bbb; }
  .log-age { color: #999; font-size: 11px; }
  .log-time { color: #bbb; font-size: 11px; text-align: right; }
  .log-empty { padding: 20px; text-align: center; font-size: 12px; color: #bbb; }

  ::-webkit-scrollbar { width: 4px; }
  ::-webkit-scrollbar-track { background: transparent; }
  ::-webkit-scrollbar-thumb { background: #ddd; border-radius: 2px; }";

const char page_body[] PROGMEM =
"<header>
  <h1>Lunar Rover Control</h1>
  <div class="conn">
    <div class="dot" id="dot"></div>
    <span id="conn-txt">Connected</span>
  </div>
</header>

<div class="layout">

  <!-- LEFT: drive -->
  <div class="left">
    <div class="section-label">Drive</div>

    <div class="dpad">
      <button class="btn btn-fwd" data-cmd="F"><span class="ico">▲</span><span class="lbl">FWD</span></button>
      <button class="btn btn-l"   data-cmd="L"><span class="ico">◀</span><span class="lbl">LEFT</span></button>
      <button class="btn btn-stp" data-cmd="S"><span class="ico" style="font-size:14px">■</span><span class="lbl">STOP</span></button>
      <button class="btn btn-r"   data-cmd="R"><span class="ico">▶</span><span class="lbl">RIGHT</span></button>
      <button class="btn btn-bk"  data-cmd="B"><span class="ico">▼</span><span class="lbl">REV</span></button>
    </div>

    <div class="speed-row">
      <div class="speed-top"><span>Speed</span><span id="spd-lbl">60%</span></div>
      <input type="range" min="20" max="100" value="60" id="spd" oninput="setSpd(this.value)">
    </div>

    <div class="kbd-hint"><kbd>W</kbd><kbd>A</kbd><kbd>S</kbd><kbd>D</kbd> to drive &nbsp; <kbd>␣</kbd> stop</div>
  </div>

  <!-- RIGHT: sensors -->
  <div class="right">
    <div class="right-inner">

      <div class="section-label">Rock Data</div>

      <div class="rock-card">
        <div class="rock-top">
          <div>
            <div class="rock-type unknown" id="rock-type">Not scanned</div>
            <div class="rock-age" id="rock-age">Age: —</div>
          </div>
        </div>
        <div class="sgrid">
          <div class="scell">
            <div class="sname">IR Rate</div>
            <div class="sval" id="s-ir">—</div>
            <div class="sunit">pulses/sec</div>
          </div>
          <div class="scell">
            <div class="sname">Radioactivity</div>
            <div class="sval" id="s-radio">—</div>
            <div class="sunit">level</div>
          </div>
          <div class="scell">
            <div class="sname">Ultrasound</div>
            <div class="sval" id="s-us">—</div>
            <div class="sunit">40 kHz</div>
          </div>
          <div class="scell">
            <div class="sname">Magnetic</div>
            <div class="sval" id="s-mag">—</div>
            <div class="sunit">polarity</div>
          </div>
        </div>
        <div class="ir-wrap">
          <div class="ir-top"><span>IR rate</span><span>threshold 430/s</span></div>
          <div class="ir-track">
            <div class="ir-fill" id="ir-fill"></div>
            <div class="ir-thresh"></div>
          </div>
        </div>
      </div>

      <button class="scan-btn" id="scan-btn" onclick="scan()">Scan Rock</button>
      <div class="prog-wrap"><div class="prog-fill" id="prog"></div></div>

      <div class="log-card">
        <div class="log-header">
          <span>Rock Log</span>
          <span id="log-count" style="color:#bbb;font-weight:400">0 rocks</span>
        </div>
        <div class="log-entries" id="log-list">
          <div class="log-empty" id="log-empty">No rocks scanned yet</div>
        </div>
      </div>

    </div>
  </div>
</div>";

const char page_js[] PROGMEM=
"const IP     = window.location.hostname || '192.168.0.11';
const CMD_MS = 150;
let speed = 60, active = null, cmdTimer = null;

// speed
function setSpd(v) {
  speed = +v;
  document.getElementById('spd-lbl').textContent = v + '%';
  document.getElementById('spd').style.setProperty('--p', v + '%');
}
setSpd(60);

// connection
function setConn(ok) {
  document.getElementById('dot').className = 'dot' + (ok ? '' : ' off');
  document.getElementById('conn-txt').textContent = ok ? 'Connected' : 'Connection lost';
}

// drive
async function sendCmd(cmd) {
  try {
    await fetch(`http://${IP}/hold?cmd=${cmd}&speed=${speed}`, { signal: AbortSignal.timeout(400) });
    setConn(true);
  } catch { setConn(false); }
}

function startCmd(cmd) {
  if (active === cmd) return;
  stopCmd();
  active = cmd;
  document.querySelector(`[data-cmd="${cmd}"]`)?.classList.add('on');
  sendCmd(cmd);
  cmdTimer = setInterval(() => sendCmd(cmd), CMD_MS);
}

function stopCmd() {
  if (!active) return;
  document.querySelector(`[data-cmd="${active}"]`)?.classList.remove('on');
  active = null;
  clearInterval(cmdTimer);
  sendCmd('S');
}

document.querySelectorAll('.btn[data-cmd]').forEach(btn => {
  const c = btn.dataset.cmd;
  btn.addEventListener('mousedown',  () => c === 'S' ? sendCmd('S') : startCmd(c));
  btn.addEventListener('touchstart', e => { e.preventDefault(); c === 'S' ? sendCmd('S') : startCmd(c); });
  btn.addEventListener('mouseup',    () => c !== 'S' && stopCmd());
  btn.addEventListener('touchend',   () => c !== 'S' && stopCmd());
  btn.addEventListener('mouseleave', () => c !== 'S' && active === c && stopCmd());
});

const keyMap = { w:'F',a:'L',s:'B',d:'R',W:'F',A:'L',S:'B',D:'R',ArrowUp:'F',ArrowLeft:'L',ArrowDown:'B',ArrowRight:'R' };
const held = new Set();
document.addEventListener('keydown', e => {
  if (e.key === ' ') { e.preventDefault(); sendCmd('S'); return; }
  const c = keyMap[e.key];
  if (c && !held.has(e.key)) { held.add(e.key); startCmd(c); }
});
document.addEventListener('keyup', e => {
  if (keyMap[e.key]) { held.delete(e.key); if (!held.size) stopCmd(); }
});

// classify
function classify(ir, us, mag) {
  const high = ir > 430, hasUS = us === 'Present', up = mag === 'U';
  if ( high && hasUS && !up) return 'Basaltoid';
  if (!high && !hasUS && !up) return 'Gravion';
  if (!high &&  hasUS &&  up) return 'Regolix';
  if ( high && !hasUS &&  up) return 'Lunarite';
  return 'UNKNOWN';
}

// update display
function updateDisplay(ir, us, mag, age) {
  if (ir !== null) {
    document.getElementById('s-ir').textContent = ir;
    document.getElementById('s-ir').className = 'sval hi';
    document.getElementById('s-radio').textContent = ir > 430 ? 'HIGH' : 'LOW';
    document.getElementById('s-radio').className = 'sval hi';
    document.getElementById('ir-fill').style.width = Math.min(100, (ir/600)*100) + '%';
  }
  if (us !== null) {
    document.getElementById('s-us').textContent = us === 'Present' ? 'Present' : 'Absent';
    document.getElementById('s-us').className = 'sval hi';
  }
  if (mag !== null) {
    document.getElementById('s-mag').textContent = mag === 'U' ? 'Up' : 'Down';
    document.getElementById('s-mag').className = 'sval hi';
  }
  if (age !== null && age.startsWith('#')) {
    document.getElementById('rock-age').textContent = 'Age: ' + age[1] + '.' + age.slice(2) + ' Ga';
  }
  if (ir !== null && us !== null && mag !== null) {
    const type = classify(ir, us, mag);
    const el = document.getElementById('rock-type');
    el.textContent = type;
    el.className = 'rock-type ' + (type === 'UNKNOWN' ? 'unknown' : type);
    return type;
  }
  return null;
}

// scan
let rockLog = [], scanning = false;

async function scan() {
  if (scanning) return;
  scanning = true;
  const btn = document.getElementById('scan-btn');
  const prog = document.getElementById('prog');
  btn.disabled = true;
  btn.classList.add('scanning');
  btn.textContent = 'Scanning...';
  prog.style.width = '0%';

  let ir = null, us = null, mag = null, age = null;

  try {
    await fetch(`http://${IP}/IR`, { signal: AbortSignal.timeout(1000) });
    prog.style.width = '10%';

    const [usR, magR, ageR] = await Promise.all([
      fetch(`http://${IP}/US`,  { signal: AbortSignal.timeout(1000) }).then(r => r.text()),
      fetch(`http://${IP}/mag`, { signal: AbortSignal.timeout(1000) }).then(r => r.text()),
      fetch(`http://${IP}/age`, { signal: AbortSignal.timeout(1000) }).then(r => r.text()),
    ]);
    us = usR.trim(); mag = magR.trim(); age = ageR.trim();
    prog.style.width = '30%';
    updateDisplay(null, us, mag, age);

    const t0 = Date.now();
    while (Date.now() - t0 < 8000) {
      await new Promise(r => setTimeout(r, 300));
      const v = parseInt((await fetch(`http://${IP}/IRresult`, { signal: AbortSignal.timeout(500) }).then(r => r.text())).trim());
      prog.style.width = (30 + ((Date.now()-t0)/8000)*65) + '%';
      if (v !== -1) { ir = v; break; }
    }

    prog.style.width = '100%';
    const type = updateDisplay(ir, us, mag, age);

    if (type) {
      const ageStr = age?.startsWith('#') ? age[1] + '.' + age.slice(2) + ' Ga' : '—';
      rockLog.unshift({ num: rockLog.length+1, type, age: ageStr, time: new Date().toLocaleTimeString() });
      renderLog();
    }
  } catch { setConn(false); }

  setTimeout(() => {
    btn.disabled = false;
    btn.classList.remove('scanning');
    btn.textContent = 'Scan Rock';
    prog.style.width = '0%';
  }, 500);
  scanning = false;
}

function renderLog() {
  document.getElementById('log-count').textContent = rockLog.length + ' rocks';
  document.getElementById('log-empty')?.remove();
  document.getElementById('log-list').innerHTML = rockLog.map(e => `
    <div class="log-row">
      <span class="log-n">#${e.num}</span>
      <span class="log-t ${e.type}">${e.type}</span>
      <span class="log-age">${e.age}</span>
      <span class="log-time">${e.time}</span>
    </div>
  `).join('');
}";

WiFiWebServer server(80);

//Return the web page
void handleRoot(){
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, F("text/html"), F(""));
  server.sendContent(FPSTR(page_head));
  server.sendContent(FPSTR(page_body));
  server.sendContent(FPSTR(page_js));
}

void handleMag(){
  char result = readMagnetic();
  server.send(200, F("text/plain"),String(result));
}

void handleIR(){
  if(measuring_IR){
    server.send(200, F("text/plain"), F("Busy"));
    return;
  }
  noInterrupts();
  pulseCount = 0;
  interrupts();
  windowStart = millis();
  measuring_IR = true;
  IR_emission_category = -1;
  attachInterrupt(digitalPinToInterrupt(IR_PIN), onPulseDetected, FALLING);
  server.send(200, F("text/plain"), F("Measuring...")); 
}

void handleIRresult(){
  server.send(200, F("text/plain"), String(IR_emission_category));
}

void handleUS(){
  bool result = readUltrasound();
  if(result == true){
    server.send(200, F("text/plain"), F("Present"));
  }
  else{
    server.send(200, F("text/plain"), F("Absent"));
  }
}

void handleAge(){
  String result = readAge();
  server.send(200, F("text/plain"), String(result));
}

void runCommand(char cmd){
  LastCommandTime = millis();
  switch(cmd){
    case 'F': DriveForward(); MotorState = true; break;
    case 'B' : DriveBackward(); MotorState = true; break;
    case 'L' : TurnLeft(); MotorState = true; break;
    case 'R' : TurnRight(); MotorState = true;  break;
    default: DriveStop(); MotorState = false; break;
  }

}

void handleHold(){
  String cmd = server.arg("cmd");
  char c;
  if(cmd.length() > 0){
    c = cmd[0];
  }
  else{
    c = 'S';
  }
  runCommand(c);
  server.send(200,F("text/plain"),F("OK"));

}


//Generate a 404 response with details of the failed request
void handleNotFound()
{
  String message = F("File Not Found\n\n"); 
  message += F("URI: ");
  message += server.uri();
  message += F("\nMethod: ");
  message += (server.method() == HTTP_GET) ? F("GET") : F("POST");
  message += F("\nArguments: ");
  message += server.args();
  message += F("\n");
  for (uint8_t i = 0; i < server.args(); i++)
  {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, F("text/plain"), message);
}

void setup()
{
  pinMode(LEFT_PWM, OUTPUT);
  pinMode(LEFT_DIR, OUTPUT);
  pinMode(RIGHT_PWM, OUTPUT);
  pinMode(RIGHT_DIR, OUTPUT);
  DriveStop();

  //pin mode for IR sensor
  pinMode(IR_PIN, INPUT_PULLUP);

  Serial.begin(9600);
  Serial1.begin(600);

  //Wait 10s for the serial connection before proceeding
  //This ensures you can see messages from startup() on the monitor
  //Remove this for faster startup when the USB host isn't attached
  while (!Serial && millis() < 10000);  

  Serial.println(F("\nStarting Web Server"));

  //Check WiFi shield is present
  if (WiFi.status() == WL_NO_SHIELD)
  {
    Serial.println(F("WiFi shield not present"));
    while (true);
  }

  //Configure the static IP address if group number is set
  if (groupNumber)
    WiFi.config(IPAddress(192,168,0,groupNumber+1));

  // attempt to connect to WiFi network
  Serial.print(F("Connecting to WPA SSID: "));
  Serial.println(ssid);
  while (WiFi.begin(ssid, pass) != WL_CONNECTED)
  {
    delay(500);
    Serial.print('.');
  }

  if (bmm150.begin() != 0) {
  Serial.println(F("BMM150 init failed"));
  } else {
    Serial.println(F("BMM150 init success"));
    bmm150.setOperationMode(BMM150_POWERMODE_NORMAL);
    bmm150.setPresetMode(BMM150_PRESETMODE_HIGHACCURACY);
    bmm150.setRate(BMM150_DATA_RATE_10HZ);
    bmm150.setMeasurementXYZ();
  }

  //Register the callbacks to respond to HTTP requests
  server.on(F("/hold"), handleHold);
  server.on(F("/"), handleRoot);
  server.on(F("/mag"), handleMag);
  server.on(F("/IR"), handleIR);
  server.on(F("/IRresult"), handleIRresult);
  server.on(F("/US"), handleUS);
  server.on(F("/age"), handleAge);
 

  server.onNotFound(handleNotFound);
  
  server.begin();
  
  Serial.print(F("HTTP server started @ "));
  Serial.println(static_cast<IPAddress>(WiFi.localIP()));
}

//Call the server polling function in the main loop
void loop(){
  server.handleClient();
  if((MotorState == true)&&(millis() - LastCommandTime > 500)){
    MotorState = false;
    DriveStop();
  }

  if(measuring_IR&&(millis() - windowStart >= IR_MEASURE_WINDOW_MS)){
    detachInterrupt(digitalPinToInterrupt(IR_PIN));
      noInterrupts();
      uint32_t count = pulseCount;
      interrupts();
      float measuredHz = (float)count / (IR_MEASURE_WINDOW_MS / 1000.0f);
    if(measuredHz<100||measuredHz>800){
      IR_emission_category = 0;
    }
    else{
      if(abs(312-measuredHz)<abs(547-measuredHz)){
        IR_emission_category = 312;
      }
      else{
      IR_emission_category = 547;
      }
    } 
    measuring_IR = false;
  }

}
