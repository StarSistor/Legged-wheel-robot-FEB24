#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <WiFiManager.h>

// Pin Definitions
const int AIA = 4;        // PWM pin 1 connected to A-IA
const int AIB = 5;        // PWM pin 2 connected to A-IB
const int BIA = 2;        // PWM pin 3 connected to B-IA
const int BIB = 0;        // PWM pin 4 connected to B-IB
const int ABIA_leg = 14;  // Legged control pin A
const int ABIB_leg = 12;  // Legged control pin B

// Variables
unsigned long previousMillis = 0;
const long interval = 100;
int sliderValue = 0;
int joystickXValue = 0;
int joystickYValue = 0;
bool switchValue = false;
bool lastSwitchState = false;
int rawX = 0;
int rawY = 0;
int deadZone = 40; // Defines the dead zone size

// Create a Web Server instance
ESP8266WebServer server(80);

void setup() {
  Serial.begin(115200);

  // Configure WiFiManager
  WiFiManager wifiManager;
  wifiManager.autoConnect("AutoConnectAP");

  Serial.println("Waiting for Wi-Fi connection...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("WiFi connected");

  // Initialize pins
  pinMode(AIA, OUTPUT);
  pinMode(AIB, OUTPUT);
  pinMode(BIA, OUTPUT);
  pinMode(BIB, OUTPUT);
  pinMode(ABIA_leg, OUTPUT);
  pinMode(ABIB_leg, OUTPUT);

  // Stop all movements
  stopMotors();
  stopLegged();
  
  // Set up routes
  server.on("/", handleRoot);
  server.on("/slider", handleSlider);
  server.on("/joystick", HTTP_GET, handleJoystick);
  server.on("/switch", handleSwitch);

  // Start server
  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  int valueX = rawX;
  int valueY = rawY;

  // Apply dead zone
  if (abs(valueX) < deadZone) valueX = 0;
  if (abs(valueY) < deadZone) valueY = 0;

  // Control motors based on joystick position
  if (valueY < 0) {
    forward();
  } else if (valueX > 0) {
    right();
  } else if (valueX < 0) {
    left();
  } else if (valueX == 0 && valueY == 0) {
    stopMotors();
  }

  // Handle legged control based on switch state
  if (switchValue != lastSwitchState) {
    previousMillis = millis();
    if (switchValue) {
      activateLegged();
    } else {
      deactivateLegged();
    }
    lastSwitchState = switchValue;
  } else {
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= 2 * sliderValue) {
      stopLegged();
    }
  }

  // Handle incoming client requests
  server.handleClient();
}

// Handlers for different routes

void handleRoot() {
  String html = "<!DOCTYPE html><html><head><style>body { display: flex; justify-content: center; align-items: center; height: 100vh; flex-direction: column; }</style><script>";
  html += "function updateJoystick(event) {";
  html += "  var joystick = document.getElementById('joystick');";
  html += "  var joystickDot = document.getElementById('joystickDot');";
  html += "  var rect = joystick.getBoundingClientRect();";
  html += "  var x = event.clientX - rect.left;";
  html += "  var y = event.clientY - rect.top;";
  html += "  var radius = joystick.clientWidth / 2;";
  html += "  var angle = Math.atan2(y - radius, x - radius);";
  html += "  var distance = Math.min(radius, Math.hypot(x - radius, y - radius));";
  html += "  var newX = radius + distance * Math.cos(angle);";
  html += "  var newY = radius + distance * Math.sin(angle);";
  html += "  joystickDot.style.left = newX + 'px';";
  html += "  joystickDot.style.top = newY + 'px';";
  html += "  var normalizedX = (newX - radius) / radius * 100;";
  html += "  var normalizedY = (newY - radius) / radius * 100;";
  html += "  fetch('/joystick?x=' + normalizedX + '&y=' + normalizedY);";
  html += "}";
  html += "</script></head><body>";
  html += "<h2>NodeMCU Controller</h2>";
  html += "<p>Slider: <input type='range' id='slider' name='slider' min='0' max='255' onchange='fetch(\"/slider?value=\" + this.value)'></p>";
  html += "<div class='joystick' id='joystick' style='width: 200px; height: 200px; background-color: #f0f0f0; border-radius: 50%; position: relative;' onmousemove='updateJoystick(event)'>";
  html += "  <div id='joystickDot' style='width: 20px; height: 20px; background-color: #5B196A; border-radius: 50%; position: absolute; top: " + String(joystickYValue) + "%; left: " + String(joystickXValue) + "%; transform: translate(-50%, -50%);'></div>";
  html += "</div>";
  html += "<p>Switch: <input type='checkbox' id='switch' name='switch' onchange='fetch(\"/switch?value=\" + (this.checked ? 1 : 0))'></p>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

void handleSlider() {
  sliderValue = server.arg("value").toInt();  // Get the value of the slider
  server.send(200, "text/plain", "Slider value updated");  // Send a response to the client
}

void handleJoystick() {
  rawX = server.arg("x").toInt();  // Get the raw X value of the joystick
  rawY = server.arg("y").toInt();  // Get the raw Y value of the joystick
  server.send(200, "text/plain", "Joystick value updated");  // Send a response to the client
}

void handleSwitch() {
  switchValue = server.arg("value").toInt() == 1;  // Get the value of the switch
  server.send(200, "text/plain", "Switch value updated");  // Send a response to the client
}

// Motor control functions

void forward() {
  digitalWrite(AIA, LOW);
  digitalWrite(AIB, HIGH);
  digitalWrite(BIA, LOW);
  digitalWrite(BIB, HIGH);
}

void right() {
  digitalWrite(AIA, LOW);
  digitalWrite(AIB, HIGH);
  digitalWrite(BIA, LOW);
  digitalWrite(BIB, LOW);
}

void left() {
  digitalWrite(AIA, LOW);
  digitalWrite(AIB, LOW);
  digitalWrite(BIA, LOW);
  digitalWrite(BIB, HIGH);
}

void stopMotors() {
  digitalWrite(AIA, LOW);
  digitalWrite(AIB, LOW);
  digitalWrite(BIA, LOW);
  digitalWrite(BIB, LOW);
}

// Legged control functions

void deactivateLegged() {
  digitalWrite(ABIA_leg, HIGH);
  digitalWrite(ABIB_leg, LOW);
}

void activateLegged() {
  digitalWrite(ABIA_leg, LOW);
  digitalWrite(ABIB_leg, HIGH);
}

void stopLegged() {
  digitalWrite(ABIA_leg, LOW);
  digitalWrite(ABIB_leg, LOW);
}





//////////////




#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <WiFiManager.h>

// Pin Definitions
const int AIA = 4;        // PWM pin 1 connected to A-IA
const int AIB = 5;        // PWM pin 2 connected to A-IB
const int BIA = 2;        // PWM pin 3 connected to B-IA
const int BIB = 0;        // PWM pin 4 connected to B-IB
const int ABIA_leg = 14;  // Legged control pin A
const int ABIB_leg = 12;  // Legged control pin B

// Variables
unsigned long previousMillis = 0;
const long interval = 100;
int sliderValue = 0;
int joystickXValue = 0;
int joystickYValue = 0;
bool switchValue = false;
bool lastSwitchState = false;
int rawX = 0;
int rawY = 0;
int deadZone = 40; // Defines the dead zone size

// Create a Web Server instance
ESP8266WebServer server(80);

void setup() {
  Serial.begin(115200);

  // Configure WiFiManager
  WiFiManager wifiManager;
  wifiManager.autoConnect("AutoConnectAP");

  Serial.println("Waiting for Wi-Fi connection...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("WiFi connected");

  // Initialize pins
  pinMode(AIA, OUTPUT);
  pinMode(AIB, OUTPUT);
  pinMode(BIA, OUTPUT);
  pinMode(BIB, OUTPUT);
  pinMode(ABIA_leg, OUTPUT);
  pinMode(ABIB_leg, OUTPUT);

  // Stop all movements
  stopMotors();
  stopLegged();
  
  // Set up routes
  server.on("/", handleRoot);
  server.on("/slider", handleSlider);
  server.on("/joystick", HTTP_GET, handleJoystick);
  server.on("/switch", handleSwitch);

  // Start server
  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  int valueX = rawX;
  int valueY = rawY;

  // Apply dead zone
  if (abs(valueX) < deadZone) valueX = 0;
  if (abs(valueY) < deadZone) valueY = 0;

  // Control motors based on joystick position
  if (valueY < 0) {
    forward();
  } else if (valueX > 0) {
    right();
  } else if (valueX < 0) {
    left();
  } else if (valueX == 0 && valueY == 0) {
    stopMotors();
  }

  // Handle legged control based on switch state
  if (switchValue != lastSwitchState) {
    previousMillis = millis();
    if (switchValue) {
      activateLegged();
    } else {
      deactivateLegged();
    }
    lastSwitchState = switchValue;
  } else {
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= 2 * sliderValue) {
      stopLegged();
    }
  }

  // Handle incoming client requests
  server.handleClient();
}

// Handlers for different routes

void handleRoot() {
  String html = "<!DOCTYPE html><html><head><style>body { display: flex; justify-content: center; align-items: center; height: 100vh; flex-direction: column; }</style><script>";
  html += "function updateJoystick(event) {";
  html += "  var joystick = document.getElementById('joystick');";
  html += "  var joystickDot = document.getElementById('joystickDot');";
  html += "  var rect = joystick.getBoundingClientRect();";
  html += "  var x = event.clientX - rect.left;";
  html += "  var y = event.clientY - rect.top;";
  html += "  var radius = joystick.clientWidth / 2;";
  html += "  var angle = Math.atan2(y - radius, x - radius);";
  html += "  var distance = Math.min(radius, Math.hypot(x - radius, y - radius));";
  html += "  var newX = radius + distance * Math.cos(angle);";
  html += "  var newY = radius + distance * Math.sin(angle);";
  html += "  joystickDot.style.left = newX + 'px';";
  html += "  joystickDot.style.top = newY + 'px';";
  html += "  var normalizedX = (newX - radius) / radius * 100;";
  html += "  var normalizedY = (newY - radius) / radius * 100;";
  html += "  fetch('/joystick?x=' + normalizedX + '&y=' + normalizedY);";
  html += "}";
  html += "</script></head><body>";
  html += "<h2>NodeMCU Controller</h2>";
  html += "<p>Slider: <input type='range' id='slider' name='slider' min='0' max='255' onchange='fetch(\"/slider?value=\" + this.value)'></p>";
  html += "<div class='joystick' id='joystick' style='width: 200px; height: 200px; background-color: #f0f0f0; border-radius: 50%; position: relative;' onmousemove='updateJoystick(event)'>";
  html += "  <div id='joystickDot' style='width: 20px; height: 20px; background-color: #5B196A; border-radius: 50%; position: absolute; top: " + String(joystickYValue) + "%; left: " + String(joystickXValue) + "%; transform: translate(-50%, -50%);'></div>";
  html += "</div>";
  html += "<p>Switch: <input type='checkbox' id='switch' name='switch' onchange='fetch(\"/switch?value=\" + (this.checked ? 1 : 0))'></p>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

void handleSlider() {
  sliderValue = server.arg("value").toInt();  // Get the value of the slider
  server.send(200, "text/plain", "Slider value updated");  // Send a response to the client
}

void handleJoystick() {
  rawX = server.arg("x").toInt();  // Get the raw X value of the joystick
  rawY = server.arg("y").toInt();  // Get the raw Y value of the joystick
  server.send(200, "text/plain", "Joystick value updated");  // Send a response to the client
}

void handleSwitch() {
  switchValue = server.arg("value").toInt() == 1;  // Get the value of the switch
  server.send(200, "text/plain", "Switch value updated");  // Send a response to the client
}

// Motor control functions

void forward() {
  digitalWrite(AIA, LOW);
  digitalWrite(AIB, HIGH);
  digitalWrite(BIA, LOW);
  digitalWrite(BIB, HIGH);
}

void right() {
  digitalWrite(AIA, LOW);
  digitalWrite(AIB, HIGH);
  digitalWrite(BIA, LOW);
  digitalWrite(BIB, LOW);
}

void left() {
  digitalWrite(AIA, LOW);
  digitalWrite(AIB, LOW);
  digitalWrite(BIA, LOW);
  digitalWrite(BIB, HIGH);
}

void stopMotors() {
  digitalWrite(AIA, LOW);
  digitalWrite(AIB, LOW);
  digitalWrite(BIA, LOW);
  digitalWrite(BIB, LOW);
}

// Legged control functions

void deactivateLegged() {
  digitalWrite(ABIA_leg, HIGH);
  digitalWrite(ABIB_leg, LOW);
}

void activateLegged() {
  digitalWrite(ABIA_leg, LOW);
  digitalWrite(ABIB_leg, HIGH);
}

void stopLegged() {
  digitalWrite(ABIA_leg, LOW);
  digitalWrite(ABIB_leg, LOW);
}
