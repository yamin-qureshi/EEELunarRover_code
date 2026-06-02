#define USE_WIFI_NINA         false
#define USE_WIFI101           true
#include <WiFiWebServer.h>
#include <Wire.h>
#include "DFRobot_BMM150.h"

#ifndef FPSTR
#define FPSTR(pstr) (reinterpret_cast<const __FlashStringHelper *>(pstr))
#endif

const char ssid[] = "xxxxxxxxxx";
const char pass[] = "wifipassword";
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
//Webpage to return when root is requested

char readMagnetic() {
  sBmm150MagData_t magData = bmm150.getGeomagneticData();
  
  if(magData.z < 0){
    return 'D';
  }
  else{
    return 'U';
  }
}

const char page_head[] PROGMEM =
"<html><head><style>\
.btn {background-color: inherit;padding: 14px 28px;font-size: 16px;}\
.btn:hover {background: #eee;}\
</style></head><body>";

const char page_body[] PROGMEM =
"<button class=\"btn\" onmousedown=\"startForward()\" onmouseup=\"stopDriving()\">Forward</button>\
<button class=\"btn\" onmousedown=\"startBackward()\" onmouseup=\"stopDriving()\">Backward</button>\
<button class=\"btn\" onmousedown=\"startLeft()\" onmouseup=\"stopDriving()\">Left</button>\
<button class=\"btn\" onmousedown=\"startRight()\" onmouseup=\"stopDriving()\">Right</button>\
<button class=\"btn\" onclick=\"stopDriving()\">Stop</button>\
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
\
</script></html>";

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

  Serial.begin(9600);

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
}

