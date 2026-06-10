//new UI
#define USE_WIFI_NINA         false
#define USE_WIFI101           true
#include <WiFiWebServer.h>
#include <Wire.h>
#include "DFRobot_BMM150.h"
#define IR_PIN  2 

const int IR_MEASURE_WINDOW_MS =  600;
volatile uint32_t pulseCount = 0; 
uint32_t windowStart; 
void onPulseDetected() {
  pulseCount++;
}
int IR_emission_category = -1; //to inform us whether the rock is emitting IR at 312/s or 547/s, -1 means IR not ready, 0 means invalid
bool measuring_IR = false;

int US_pin = A0;

const int AGE_MEASURE_WINDOW_MS = 200;

bool scanning = false;
String scanType = "Not ready";
int scanIR = -1;
String scanUS = "-";
char scanMag = '-';
String scanAge = "-";

#ifndef FPSTR
#define FPSTR(pstr) (reinterpret_cast<const __FlashStringHelper *>(pstr))
#endif


const char ssid[] = "mnyq";
const char pass[] = "12345678";
const int groupNumber = 0; // Set your group number to make the IP address constant - only do this on the EEERover network

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
  while(Serial1.available()) Serial1.read();

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

String classifyRock(){
  if(IR_emission_category == -1){
    return "IR not ready";
  }
  if(IR_emission_category == 0){
    return "IR invalid";
  }
  char mag = readMagnetic();
  bool US = readUltrasound();

  if(US == true){
    if(IR_emission_category == 547 && mag == 'D'){
      return "Basaltoid";
    }
    if(IR_emission_category == 312 && mag == 'U'){
      return "Regolix";
    }
  }
  else{
    if(IR_emission_category == 312 && mag == 'D'){
      return "Gravion";
    }
    if(IR_emission_category == 547 && mag == 'U'){
      return "Lunarite";
    }
  }

  return "Sensor conflict";
  
}


const char page_head1[] PROGMEM =
"<html><head><meta name='viewport' content='width=device-width, initial-scale=1'><style>"
"*{ box-sizing: border-box; }"
"body{ font-family: -apple-system, Segoe UI, Roboto, sans-serif; background: #f4f6f8; color: #1c2530; margin: 0 auto; padding: 24px 16px; max-width: 420px; }"
".header{ text-align: center; margin-bottom: 24px; }"
".header h1{ font-size: 1.4em; font-weight: 600; margin: 0; }"
".header p{ font-size: 0.8em; color: #7a8694; margin: 4px 0 0; letter-spacing: 1px; text-transform: uppercase; }";

const char page_head2[] PROGMEM =
".drive-grid{ display: grid; grid-template-columns: repeat(3, 1fr); grid-template-rows: repeat(3, 72px); gap: 10px; max-width: 280px; margin: 0 auto 24px; }"
".btn{ font-size: 1em; font-weight: 500; color: #1c2530; border: 1px solid #d3dae2; border-radius: 12px; background: #fff; cursor: pointer; transition: all .1s; }"
".btn:hover{ background: #f0f3f6; }"
".btn:active{ background: #e2e8ee; transform: scale(0.96); }";

const char page_head3[] PROGMEM =
"#btn-forward{ grid-column: 2; grid-row: 1; }"
"#btn-left{ grid-column: 1; grid-row: 2; }"
"#btn-right{ grid-column: 3; grid-row: 2; }"
"#btn-back{ grid-column: 2; grid-row: 3; }"
"#btn-stop{ grid-column: 2; grid-row: 2; background: #fdecec; border-color: #f3b6b6; color: #b03030; font-weight: 600; }"
"#btn-stop:hover{ background: #fbdede; }";

const char page_head4[] PROGMEM =
"#btn-scan{ display: block; width: 100%; padding: 16px; font-size: 1.05em; font-weight: 600; color: #fff;"
" border: none; border-radius: 12px; background: #2e7d4f; cursor: pointer; margin-bottom: 24px; transition: all .1s; }"
"#btn-scan:hover{ background: #276b44; }"
"#btn-scan:active{ transform: scale(0.99); }"
"#btn-scan:disabled{ background: #c4cdd5; cursor: default; }";

const char page_head5[] PROGMEM =
".card{ background: #fff; border: 1px solid #e1e6eb; border-radius: 14px; padding: 20px; }"
".rock-type{ text-align: center; font-size: 1.5em; font-weight: 700; color: #2e7d4f; margin: 0 0 16px; }"
".card table{ width: 100%; border-collapse: collapse; }"
".card td{ padding: 10px 4px; border-bottom: 1px solid #eef1f4; font-size: 0.95em; }"
".card tr:last-child td{ border-bottom: none; }"
".card td:first-child{ color: #7a8694; }"
".card td:last-child{ text-align: right; font-weight: 500; }";

const char page_head6[] PROGMEM =
"#status{ text-align: center; font-size: 0.8em; color: #9aa5b1; margin: 14px 0 0; min-height: 1em; }"
"</style></head><body>"
"<div class='header'>"
"  <h1>Lunar Rover</h1>"
"  <p>Control Panel</p>"
"</div>";

const char page_body[] PROGMEM =
"<div class=\"drive-grid\">"
"  <button class=\"btn\" id=\"btn-forward\" onmousedown=\"startDriving('F')\" onmouseup=\"stopDriving()\" ontouchstart=\"startDriving('F');event.preventDefault()\" ontouchend=\"stopDriving()\">Forward</button>"
"  <button class=\"btn\" id=\"btn-left\" onmousedown=\"startDriving('L')\" onmouseup=\"stopDriving()\" ontouchstart=\"startDriving('L');event.preventDefault()\" ontouchend=\"stopDriving()\">Left</button>"
"  <button class=\"btn\" id=\"btn-stop\" onclick=\"stopDriving()\">Stop</button>"
"  <button class=\"btn\" id=\"btn-right\" onmousedown=\"startDriving('R')\" onmouseup=\"stopDriving()\" ontouchstart=\"startDriving('R');event.preventDefault()\" ontouchend=\"stopDriving()\">Right</button>"
"  <button class=\"btn\" id=\"btn-back\" onmousedown=\"startDriving('B')\" onmouseup=\"stopDriving()\" ontouchstart=\"startDriving('B');event.preventDefault()\" ontouchend=\"stopDriving()\">Back</button>"
"</div>";

const char page_results[] PROGMEM =
"<button id=\"btn-scan\" onclick=\"startScan()\">Scan Rock</button>"
"<div class=\"card\">"
"  <div class=\"rock-type\" id=\"rock-type\">-</div>"
"  <table>"
"    <tr><td>Infrared</td><td id=\"val-ir\">-</td></tr>"
"    <tr><td>Ultrasound</td><td id=\"val-us\">-</td></tr>"
"    <tr><td>Magnetic</td><td id=\"val-mag\">-</td></tr>"
"    <tr><td>Age</td><td id=\"val-age\">-</td></tr>"
"  </table>"
"  <p id=\"status\"></p>"
"</div></body>";

const char page_js1[] PROGMEM =
"<script>"
"var driveReq = new XMLHttpRequest();"
"var scanReq = new XMLHttpRequest();"
"var driveInterval;"
""
"function startDriving(cmd) {"
"  clearInterval(driveInterval);"
"  driveInterval = setInterval(function() {"
"    driveReq.open('GET', '/hold?cmd=' + cmd);"
"    driveReq.send();"
"  }, 150);"
"}"
""
"function stopDriving() {"
"  clearInterval(driveInterval);"
"  driveReq.open('GET', '/hold?cmd=S');"
"  driveReq.send();"
"}";

const char page_js2[] PROGMEM =
"function startScan() {"
"  document.getElementById('status').innerText = 'Scanning rock...';"
"  document.getElementById('btn-scan').disabled = true;"
"  var req = new XMLHttpRequest();"
"  req.open('GET', '/scan');"
"  req.send();"
"  setTimeout(pollForResult, 500);"
"}"
""
"function pollForResult() {"
"  scanReq.onreadystatechange = function() {"
"    if (scanReq.readyState == 4 && scanReq.status == 200) {"
"      var response = scanReq.responseText;"
"      if (response == 'Scanning' || response == 'Busy') {"
"        setTimeout(pollForResult, 500);"
"      } else {"
"        showResult(response);"
"      }"
"    }"
"  };"
"  scanReq.open('GET', '/scanResult');"
"  scanReq.send();"
"}";

const char page_js3[] PROGMEM =
"function showResult(csv) {"
"  var parts = csv.split(',');"
"  document.getElementById('rock-type').innerText = parts[0];"
"  document.getElementById('val-ir').innerText  = parts[1];"
"  document.getElementById('val-us').innerText  = parts[2];"
"  document.getElementById('val-mag').innerText = parts[3];"
"  document.getElementById('val-age').innerText = parts[4];"
"  document.getElementById('status').innerText = 'Scan complete';"
"  document.getElementById('btn-scan').disabled = false;"
"}"
"</script></html>";
WiFiWebServer server(80);

//Return the web page
void handleRoot(){
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, F("text/html"), F(""));
  server.sendContent(FPSTR(page_head1));
  server.sendContent(FPSTR(page_head2));
  server.sendContent(FPSTR(page_head3));
  server.sendContent(FPSTR(page_head4));
  server.sendContent(FPSTR(page_head5));
  server.sendContent(FPSTR(page_head6));
  server.sendContent(FPSTR(page_body));
  server.sendContent(FPSTR(page_results));
  server.sendContent(FPSTR(page_js1));
  server.sendContent(FPSTR(page_js2));
  server.sendContent(FPSTR(page_js3));
}

void handleMag(){
  char result = readMagnetic();
  server.send(200, F("text/plain"),String(result));
}

void startIRWindow(){
  noInterrupts();
  pulseCount = 0;
  interrupts();
  windowStart = millis();
  measuring_IR = true;
  IR_emission_category = -1;
  attachInterrupt(digitalPinToInterrupt(IR_PIN), onPulseDetected, FALLING);
}

void handleIR(){
  if(measuring_IR){
    server.send(200, F("text/plain"), F("Busy"));
    return;
  }
  startIRWindow();
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

void handleScan(){
  if(measuring_IR == true || scanning == true){
    server.send(200, F("text/plain"), F("Busy"));
    return;
  }
  DriveStop();
  MotorState = false;
  scanning = true;
  startIRWindow();
  server.send(200, F("text/plain"), F("Scan started"));
}

void handleScanResult(){
  if(scanning == true){
    server.send(200, F("text/plain"), F("Scanning"));
    return;
  }

  String out = scanType + "," + String(scanIR) + "," + scanUS + "," + String(scanMag) + "," + scanAge;
  server.send(200, F("text/plain"), out);

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
  while (!Serial && millis() < 2000);  

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
  server.on(F("/scan"), handleScan);
  server.on(F("/scanResult"), handleScanResult);
 

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

    if(scanning){
      scanMag = readMagnetic();
      if(readUltrasound() == true){
        scanUS = "Present";
      }
      else{
        scanUS = "Absent";
      }
      scanAge = readAge();
      scanIR = IR_emission_category;
      scanType = classifyRock();
      scanning = false;
    }
  }

  
}