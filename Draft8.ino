//fixed scanning
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

const int AGE_MEASURE_WINDOW_MS = 50;

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

  if((IR_emission_category == 547) && (US == false)){
    if(mag == 'D'){
      return "Basaltoid";
    }
    if(mag == 'U'){
      return "Lunarite";
    }
  }

  if((IR_emission_category == 312)&&(US == true)){
    if(mag == 'D'){
      return "Gravion";
    }
    if(mag == 'U'){
      return "Regolix";
    }
  }

  return "Sensor conflict";
  
}


const char page_head[] PROGMEM =
"<html><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"><style>\
.btn {background-color: inherit;padding: 14px 28px;font-size: 16px;}\
.btn:hover {background: #eee;}\
#scanBtn {background:#dff0d8;font-weight:bold;}\
#results {margin-top:20px;font-family:sans-serif;}\
#results table {border-collapse:collapse;}\
#results td {border:1px solid #ccc;padding:6px 14px;}\
#rType {margin-bottom:8px;}\
</style></head><body>";

const char page_body[] PROGMEM =
"<button class=\"btn\" onmousedown=\"startForward()\" onmouseup=\"stopDriving()\">Forward</button>\
<button class=\"btn\" onmousedown=\"startBackward()\" onmouseup=\"stopDriving()\">Backward</button>\
<button class=\"btn\" onmousedown=\"startLeft()\" onmouseup=\"stopDriving()\">Left</button>\
<button class=\"btn\" onmousedown=\"startRight()\" onmouseup=\"stopDriving()\">Right</button>\
<button class=\"btn\" onclick=\"stopDriving()\">Stop</button>\
<button class=\"btn\" id=\"scanBtn\" onclick=\"startScan()\">Scan</button>\
<div id=\"results\">\
<h3 id=\"rType\">-</h3>\
<table>\
<tr><td>IR (Hz class)</td><td id=\"rIR\">-</td></tr>\
<tr><td>Ultrasound</td><td id=\"rUS\">-</td></tr>\
<tr><td>Magnetic</td><td id=\"rMag\">-</td></tr>\
<tr><td>Age</td><td id=\"rAge\">-</td></tr>\
</table>\
<p id=\"rStatus\"></p>\
</div>\
</body>";

const char page_js[] PROGMEM=
"<script>\
var xhttp = new XMLHttpRequest();\
var intervalKey;\
\
function startForward(){\
  clearInterval(intervalKey);\
  intervalKey = setInterval(function(){\
    xhttp.open(\"GET\", \"/hold?cmd=F\");\
    xhttp.send();\
  }, 150);\
}\
\
function startBackward(){\
  clearInterval(intervalKey);\
  intervalKey = setInterval(function(){\
    xhttp.open(\"GET\", \"/hold?cmd=B\");\
    xhttp.send();\
  }, 150);\
}\
\
function startLeft(){\
  clearInterval(intervalKey);\
  intervalKey = setInterval(function(){\
    xhttp.open(\"GET\", \"/hold?cmd=L\");\
    xhttp.send();\
  }, 150);\
}\
\
function startRight(){\
  clearInterval(intervalKey);\
  intervalKey = setInterval(function(){\
    xhttp.open(\"GET\", \"/hold?cmd=R\");\
    xhttp.send();\
  }, 150);\
}\
\
function stopDriving(){\
  clearInterval(intervalKey);\
  xhttp.open(\"GET\", \"/hold?cmd=S\");\
  xhttp.send();\
}\
";

const char page_scan_js[] PROGMEM=
"var scanReq = new XMLHttpRequest();\
function startScan(){\
  clearInterval(intervalKey);\
  document.getElementById(\"rStatus\").innerHTML=\"Scanning...\";\
  document.getElementById(\"scanBtn\").disabled=true;\
  var s=new XMLHttpRequest();\
  s.open(\"GET\",\"/scan\");\
  s.send();\
  setTimeout(pollScan,500);\
}\
function pollScan(){\
  scanReq.onreadystatechange=function(){\
    if(scanReq.readyState==4&&scanReq.status==200){\
      var t=scanReq.responseText;\
      if(t==\"Scanning\"||t==\"Busy\"){\
        setTimeout(pollScan,500);\
      } else {\
        showResult(t);\
      }\
    }\
  };\
  scanReq.open(\"GET\",\"/scanResult\");\
  scanReq.send();\
}\
function showResult(t){\
  var p=t.split(\",\");\
  document.getElementById(\"rType\").innerHTML=p[0];\
  document.getElementById(\"rIR\").innerHTML=p[1];\
  document.getElementById(\"rUS\").innerHTML=p[2];\
  document.getElementById(\"rMag\").innerHTML=p[3];\
  document.getElementById(\"rAge\").innerHTML=p[4];\
  document.getElementById(\"rStatus\").innerHTML=\"Done\";\
  document.getElementById(\"scanBtn\").disabled=false;\
}\
</script></html>";

WiFiWebServer server(80);

//Return the web page
void handleRoot(){
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, F("text/html"), F(""));
  server.sendContent(FPSTR(page_head));
  server.sendContent(FPSTR(page_body));
  server.sendContent(FPSTR(page_js));
  server.sendContent(FPSTR(page_scan_js));
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