#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESP-WebSocket-Server.h> // New WebSocket library
#include "L298N.h"                // New Motor Driver library
#include "ESP32_Servo.h"          // New Servo library

// --- PIN DEFINITIONS (Safe Pins) ---
// Note: These pins are safe and avoid strapping/UART conflicts.
#define RIGHT_MOTOR_EN 2
#define RIGHT_MOTOR_IN1 23
#define RIGHT_MOTOR_IN2 13

#define LEFT_MOTOR_EN 21 // Using a different EN pin for the second motor
#define LEFT_MOTOR_IN1 18
#define LEFT_MOTOR_IN2 19

#define PAN_SERVO_PIN 14
#define TILT_SERVO_PIN 15

// --- WiFi Credentials ---
const char* ssid = "sim";
const char* password = "simple12";

// --- Library Objects ---
WebServer server(80);
WebSocketServer webSocket(81);

// Create two L298N motor objects
L298N rightMotor(RIGHT_MOTOR_EN, RIGHT_MOTOR_IN1, RIGHT_MOTOR_IN2);
L298N leftMotor(LEFT_MOTOR_EN, LEFT_MOTOR_IN1, LEFT_MOTOR_IN2);

// Create two Servo objects
Servo panServo;
Servo tiltServo;

// --- HTML Page (Unchanged) ---
const char* htmlHomePage PROGMEM = R"HTMLHOMEPAGE(
<!DOCTYPE html>
<html>
  <head>
    <meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1, user-scalable=no">
    <title>RC Car Control (Alt. Libs)</title>
    <style>
      :root{--primary-color:#0074D9;--secondary-color:#FFDC00;--text-color:#111;--background-color:#f9f9f9;--button-color:var(--primary-color);--button-shadow:rgba(0,0,0,0.3);--slider-track:#d3d3d3;--slider-thumb:var(--secondary-color)}[data-theme=dark]{--primary-color:#004e92;--secondary-color:#FFD700;--text-color:#f0f0f0;--background-color:#121212;--button-color:var(--primary-color);--button-shadow:rgba(0,0,0,0.5);--slider-track:#444;--slider-thumb:var(--secondary-color)}body{background-color:var(--background-color);color:var(--text-color);font-family:Arial,sans-serif;transition:all .3s ease}.theme-switch{position:fixed;top:10px;right:10px;display:flex;align-items:center;gap:8px}.theme-switch-checkbox{height:0;width:0;visibility:hidden}.theme-switch-label{cursor:pointer;width:50px;height:24px;background:var(--primary-color);display:block;border-radius:24px;position:relative}.theme-switch-label:after{content:'';position:absolute;top:3px;left:3px;width:18px;height:18px;background:var(--secondary-color);border-radius:18px;transition:.3s}.theme-switch-checkbox:checked + .theme-switch-label:after{left:calc(100% - 3px);transform:translateX(-100%)}.theme-icon{font-size:16px}.arrows{font-size:30px;color:var(--secondary-color)}td.button{background-color:var(--button-color);border-radius:25%;box-shadow:5px 5px var(--button-shadow);margin:10px;cursor:pointer}td.button:active{transform:translate(5px,5px);box-shadow:none}.noselect{-webkit-touch-callout:none;-webkit-user-select:none;-khtml-user-select:none;-moz-user-select:none;-ms-user-select:none;user-select:none}.slidecontainer{width:100%}.slider{-webkit-appearance:none;width:100%;height:15px;border-radius:5px;background:var(--slider-track);outline:none;opacity:.7;-webkit-transition:.2s;transition:opacity .2s}.slider:hover{opacity:1}.slider::-webkit-slider-thumb{-webkit-appearance:none;appearance:none;width:25px;height:25px;border-radius:50%;background:var(--slider-thumb);cursor:pointer}.slider::-moz-range-thumb{width:25px;height:25px;border-radius:50%;background:var(--slider-thumb);cursor:pointer}#mainTable{width:450px;margin:auto;table-layout:fixed;border-spacing:10px}.control-label{text-align:left;font-weight:bold;color:var(--text-color)}.title{color:var(--primary-color);margin-bottom:15px}@media (max-width:500px){#mainTable{width:95%}}
    </style>
  </head>
  <body class="noselect" align="center">
    <h2 class="title">RC Car Control (Alt. Libs)</h2>
    <div class="theme-switch"><span class="theme-icon">Light</span><input class="theme-switch-checkbox" type="checkbox" id="theme-switch"><label class="theme-switch-label" for="theme-switch"></label><span class="theme-icon">Dark</span></div>
    <table id="mainTable">
      <tr><td></td><td class="button" ontouchstart='sendButtonInput("MoveCar","1")' ontouchend='sendButtonInput("MoveCar","0")'><span class="arrows">⇧</span></td><td></td></tr>
      <tr><td class="button" ontouchstart='sendButtonInput("MoveCar","3")' ontouchend='sendButtonInput("MoveCar","0")'><span class="arrows">⇦</span></td><td class="button"></td><td class="button" ontouchstart='sendButtonInput("MoveCar","4")' ontouchend='sendButtonInput("MoveCar","0")'><span class="arrows">⇨</span></td></tr>
      <tr><td></td><td class="button" ontouchstart='sendButtonInput("MoveCar","2")' ontouchend='sendButtonInput("MoveCar","0")'><span class="arrows">⇩</span></td><td></td></tr>
      <tr><td colspan="3" style="height:20px"></td></tr>
      <tr><td class="control-label">Speed:</td><td colspan="2"><div class="slidecontainer"><input type="range" min="0" max="255" value="150" class="slider" id="Speed" oninput='sendButtonInput("Speed",value)'></div></td></tr>
      <tr><td class="control-label">Pan:</td><td colspan="2"><div class="slidecontainer"><input type="range" min="0" max="180" value="90" class="slider" id="Pan" oninput='sendButtonInput("Pan",value)'></div></td></tr>
      <tr><td class="control-label">Tilt:</td><td colspan="2"><div class="slidecontainer"><input type="range" min="0" max="180" value="90" class="slider" id="Tilt" oninput='sendButtonInput("Tilt",value)'></div></td></tr>
    </table>
    <script>
      var wsUrl = "ws://" + window.location.hostname + ":81/";
      var websocket;
      const themeSwitch=document.getElementById("theme-switch"),currentTheme=localStorage.getItem("theme")||(window.matchMedia("(prefers-color-scheme: dark)").matches?"dark":"light");currentTheme==="dark"&&(document.documentElement.setAttribute("data-theme","dark"),themeSwitch.checked=!0),themeSwitch.addEventListener("change",function(e){e.target.checked?(document.documentElement.setAttribute("data-theme","dark"),localStorage.setItem("theme","dark")):(document.documentElement.setAttribute("data-theme","light"),localStorage.setItem("theme","light"))});
      function initWebSocket(){websocket=new WebSocket(wsUrl);websocket.onopen=function(e){sendButtonInput("Speed",document.getElementById("Speed").value);sendButtonInput("Pan",document.getElementById("Pan").value);sendButtonInput("Tilt",document.getElementById("Tilt").value)};websocket.onclose=function(e){setTimeout(initWebSocket,2e3)};websocket.onmessage=function(e){}}
      function sendButtonInput(e,t){var n=e+","+t;websocket.send(n)}window.onload=initWebSocket,document.getElementById("mainTable").addEventListener("touchend",function(e){e.preventDefault()});
    </script>
  </body>
</html>
)HTMLHOMEPAGE";

void onWebSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      Serial.printf("[%u] Disconnected!\n", num);
      rightMotor.stop();
      leftMotor.stop();
      panServo.write(90);
      tiltServo.write(90);
      break;

    case WStype_CONNECTED: {
      IPAddress ip = webSocket.remoteIP(num);
      Serial.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);
      break;
    }

    case WStype_TEXT: {
      // Create a std::string from the payload
      std::string data((char*)payload, length);
      
      // Find the comma separator
      size_t commaPos = data.find(',');
      if (commaPos != std::string::npos) {
        std::string key = data.substr(0, commaPos);
        std::string valueStr = data.substr(commaPos + 1);
        int value = atoi(valueStr.c_str());

        Serial.printf("Key: %s, Value: %d\n", key.c_str(), value);

        if (key == "MoveCar") {
            switch (value) {
                case 1: // UP
                    rightMotor.forward();
                    leftMotor.forward();
                    break;
                case 2: // DOWN
                    rightMotor.backward();
                    leftMotor.backward();
                    break;
                case 3: // LEFT
                    rightMotor.forward();
                    leftMotor.backward();
                    break;
                case 4: // RIGHT
                    rightMotor.backward();
                    leftMotor.forward();
                    break;
                case 0: // STOP
                    rightMotor.stop();
                    leftMotor.stop();
                    break;
            }
        } else if (key == "Speed") {
            rightMotor.setSpeed(value);
            leftMotor.setSpeed(value);
        } else if (key == "Pan") {
            panServo.write(value);
        } else if (key == "Tilt") {
            tiltServo.write(value);
        }
      }
      break;
    }

    case WStype_BIN:
    case WStype_ERROR:
    case WStype_FRAGMENT_TEXT_START:
    case WStype_FRAGMENT_BIN_START:
    case WStype_FRAGMENT:
    case WStype_FRAGMENT_FIN:
      break;
  }
}

void handleRoot() {
  server.send_P(200, "text/html", htmlHomePage);
}

void handleNotFound() {
  server.send(404, "text/plain", "Not found");
}

void setup() {
  Serial.begin(115200);

  // --- Attach Servos ---
  panServo.attach(PAN_SERVO_PIN);
  tiltServo.attach(TILT_SERVO_PIN);

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

  // --- Setup Web Server ---
  server.on("/", HTTP_GET, handleRoot);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("HTTP server started on port 80");

  // --- Setup WebSocket Server ---
  webSocket.begin();
  webSocket.onEvent(onWebSocketEvent);
  Serial.println("WebSocket server started on port 81");
}

void loop() {
  webSocket.loop(); // Must be called continuously
  server.handleClient(); // Handles incoming HTTP requests
}
