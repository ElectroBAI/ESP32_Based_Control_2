#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>           // Standard web server library
#include "L298N.h"               // Motor driver library
#include "ESP32_Servo.h"         // Servo library

// --- PIN DEFINITIONS (Safe Pins) ---
#define RIGHT_MOTOR_EN 2
#define RIGHT_MOTOR_IN1 23
#define RIGHT_MOTOR_IN2 13

#define LEFT_MOTOR_EN 21
#define LEFT_MOTOR_IN1 18
#define LEFT_MOTOR_IN2 19

#define PAN_SERVO_PIN 14
#define TILT_SERVO_PIN 15

// --- WiFi Credentials ---
const char* ssid = "sim";
const char* password = "simple12";

// --- Library Objects ---
WebServer server(80);

// Create motor and servo objects
L298N rightMotor(RIGHT_MOTOR_EN, RIGHT_MOTOR_IN1, RIGHT_MOTOR_IN2);
L298N leftMotor(LEFT_MOTOR_EN, LEFT_MOTOR_IN1, LEFT_MOTOR_IN2);
Servo panServo;
Servo tiltServo;

// --- HTML Page (with updated JavaScript for REST API) ---
const char* htmlHomePage PROGMEM = R"HTMLHOMEPAGE(
<!DOCTYPE html>
<html>
  <head>
    <meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1, user-scalable=no">
    <title>RC Car Control (REST API)</title>
    <style>
      :root{--primary-color:#2c3e50;--secondary-color:#e67e22;--text-color:#111;--background-color:#ecf0f1;--button-color:var(--primary-color);--button-shadow:rgba(0,0,0,0.3);--slider-track:#bdc3c7;--slider-thumb:var(--secondary-color)}[data-theme=dark]{--primary-color:#34495e;--secondary-color:#f39c12;--text-color:#ecf0f1;--background-color:#2c3e50;--button-color:var(--primary-color);--button-shadow:rgba(0,0,0,0.5);--slider-track:#7f8c8d;--slider-thumb:var(--secondary-color)}body{background-color:var(--background-color);color:var(--text-color);font-family:Arial,sans-serif;transition:all .3s ease}.theme-switch{position:fixed;top:10px;right:10px;display:flex;align-items:center;gap:8px}.theme-switch-checkbox{height:0;width:0;visibility:hidden}.theme-switch-label{cursor:pointer;width:50px;height:24px;background:var(--primary-color);display:block;border-radius:24px;position:relative}.theme-switch-label:after{content:'';position:absolute;top:3px;left:3px;width:18px;height:18px;background:var(--secondary-color);border-radius:18px;transition:.3s}.theme-switch-checkbox:checked + .theme-switch-label:after{left:calc(100% - 3px);transform:translateX(-100%)}.theme-icon{font-size:16px}.arrows{font-size:30px;color:var(--secondary-color)}td.button{background-color:var(--button-color);border-radius:25%;box-shadow:5px 5px var(--button-shadow);margin:10px;cursor:pointer}td.button:active{transform:translate(5px,5px);box-shadow:none}.noselect{-webkit-touch-callout:none;-webkit-user-select:none;-khtml-user-select:none;-moz-user-select:none;-ms-user-select:none;user-select:none}.slidecontainer{width:100%}.slider{-webkit-appearance:none;width:100%;height:15px;border-radius:5px;background:var(--slider-track);outline:none;opacity:.7;-webkit-transition:.2s;transition:opacity .2s}.slider:hover{opacity:1}.slider::-webkit-slider-thumb{-webkit-appearance:none;appearance:none;width:25px;height:25px;border-radius:50%;background:var(--slider-thumb);cursor:pointer}.slider::-moz-range-thumb{width:25px;height:25px;border-radius:50%;background:var(--slider-thumb);cursor:pointer}#mainTable{width:450px;margin:auto;table-layout:fixed;border-spacing:10px}.control-label{text-align:left;font-weight:bold;color:var(--text-color)}.title{color:var(--primary-color);margin-bottom:15px}@media (max-width:500px){#mainTable{width:95%}}
    </style>
  </head>
  <body class="noselect" align="center">
    <h2 class="title">RC Car Control (REST API)</h2>
    <div class="theme-switch"><span class="theme-icon">Light</span><input class="theme-switch-checkbox" type="checkbox" id="theme-switch"><label class="theme-switch-label" for="theme-switch"></label><span class="theme-icon">Dark</span></div>
    <table id="mainTable">
      <tr><td></td><td class="button" ontouchstart='sendCommand("/api/move/forward")' ontouchend='sendCommand("/api/move/stop")'><span class="arrows">⇧</span></td><td></td></tr>
      <tr><td class="button" ontouchstart='sendCommand("/api/move/left")' ontouchend='sendCommand("/api/move/stop")'><span class="arrows">⇦</span></td><td></td><td class="button" ontouchstart='sendCommand("/api/move/right")' ontouchend='sendCommand("/api/move/stop")'><span class="arrows">⇨</span></td></tr>
      <tr><td></td><td class="button" ontouchstart='sendCommand("/api/move/backward")' ontouchend='sendCommand("/api/move/stop")'><span class="arrows">⇩</span></td><td></td></tr>
      <tr><td colspan="3" style="height:20px"></td></tr>
      <tr><td class="control-label">Speed:</td><td colspan="2"><div class="slidecontainer"><input type="range" min="0" max="255" value="150" class="slider" id="Speed" oninput='sendCommand("/api/speed?value=" + this.value)'></div></td></tr>
      <tr><td class="control-label">Pan:</td><td colspan="2"><div class="slidecontainer"><input type="range" min="0" max="180" value="90" class="slider" id="Pan" oninput='sendCommand("/api/pan?value=" + this.value)'></div></td></tr>
      <tr><td class="control-label">Tilt:</td><td colspan="2"><div class="slidecontainer"><input type="range" min="0" max="180" value="90" class="slider" id="Tilt" oninput='sendCommand("/api/tilt?value=" + this.value)'></div></td></tr>
    </table>
    <script>
      const themeSwitch=document.getElementById("theme-switch"),currentTheme=localStorage.getItem("theme")||(window.matchMedia("(prefers-color-scheme: dark)").matches?"dark":"light");currentTheme==="dark"&&(document.documentElement.setAttribute("data-theme","dark"),themeSwitch.checked=!0),themeSwitch.addEventListener("change",function(e){e.target.checked?(document.documentElement.setAttribute("data-theme","dark"),localStorage.setItem("theme","dark")):(document.documentElement.setAttribute("data-theme","light"),localStorage.setItem("theme","light"))});
      function sendCommand(url){fetch(url).then(response=>console.log(`Sent: ${url}, Status: ${response.status}`)).catch(error=>console.error("Command failed:",error))}
      window.onload=function(){sendCommand("/api/speed?value="+document.getElementById("Speed").value);sendCommand("/api/pan?value="+document.getElementById("Pan").value);sendCommand("/api/tilt?value="+document.getElementById("Tilt").value)};
      document.getElementById("mainTable").addEventListener("touchend",function(e){e.preventDefault()});
    </script>
  </body>
</html>
)HTMLHOMEPAGE";

// --- API Handler Functions ---
void handleSuccess() {
  server.send(200, "application/json", "{\"status\":\"ok\"}");
}

void handleMove() {
  String direction = server.path().substring(10); // Get text after "/api/move/"
  Serial.println("Move: " + direction);
  if (direction == "forward") { rightMotor.forward(); leftMotor.forward(); }
  else if (direction == "backward") { rightMotor.backward(); leftMotor.backward(); }
  else if (direction == "left") { rightMotor.forward(); leftMotor.backward(); }
  else if (direction == "right") { rightMotor.backward(); leftMotor.forward(); }
  else if (direction == "stop") { rightMotor.stop(); leftMotor.stop(); }
  handleSuccess();
}

void handleSpeed() {
  if (server.hasArg("value")) {
    int speed = server.arg("value").toInt();
    rightMotor.setSpeed(speed);
    leftMotor.setSpeed(speed);
    Serial.println("Speed set to: " + String(speed));
    handleSuccess();
  } else {
    server.send(400, "text/plain", "Bad Request: Missing 'value' parameter");
  }
}

void handlePan() {
  if (server.hasArg("value")) {
    int angle = server.arg("value").toInt();
    panServo.write(angle);
    Serial.println("Pan set to: " + String(angle));
    handleSuccess();
  } else {
    server.send(400, "text/plain", "Bad Request: Missing 'value' parameter");
  }
}

void handleTilt() {
  if (server.hasArg("value")) {
    int angle = server.arg("value").toInt();
    tiltServo.write(angle);
    Serial.println("Tilt set to: " + String(angle));
    handleSuccess();
  } else {
    server.send(400, "text/plain", "Bad Request: Missing 'value' parameter");
  }
}

void handleRoot() {
  server.send_P(200, "text/html", htmlHomePage);
}

void handleNotFound() {
  server.send(404, "text/plain", "Not Found");
}

void setup() {
  Serial.begin(115200);

  // --- Attach Servos and set to default position ---
  panServo.attach(PAN_SERVO_PIN);
  tiltServo.attach(TILT_SERVO_PIN);
  panServo.write(90);
  tiltServo.write(90);

  // --- Connect to WiFi ---
  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // --- Setup Web Server and API Endpoints ---
  server.on("/", HTTP_GET, handleRoot);
  
  // Movement API endpoints
  server.on("/api/move/forward", HTTP_GET, handleMove);
  server.on("/api/move/backward", HTTP_GET, handleMove);
  server.on("/api/move/left", HTTP_GET, handleMove);
  server.on("/api/move/right", HTTP_GET, handleMove);
  server.on("/api/move/stop", HTTP_GET, handleMove);
  
  // Control API endpoints
  server.on("/api/speed", HTTP_GET, handleSpeed);
  server.on("/api/pan", HTTP_GET, handlePan);
  server.on("/api/tilt", HTTP_GET, handleTilt);

  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("HTTP server with REST API started");
}

void loop() {
  server.handleClient(); // Handles incoming HTTP requests
}
