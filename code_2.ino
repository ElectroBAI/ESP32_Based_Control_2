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

// --- HTML Page with Stop Button and Theme Toggle ---
const char* htmlHomePage PROGMEM = R"HTMLHOMEPAGE(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
    <title>ESP32 RC Car</title>
    <style>
        :root {
            --bg-color: #e0e5ec;
            --main-color: #007bff;
            --accent-color: #dc3545; /* Red for Stop */
            --text-color: #333;
            --card-bg: rgba(255, 255, 255, 0.6);
            --shadow-light: #ffffff;
            --shadow-dark: #a3b1c6;
            --thumb-color: var(--main-color);
            --track-color: rgba(0, 0, 0, 0.1);
        }

        [data-theme="dark"] {
            --bg-color: #1e1e1e;
            --main-color: #00aaff;
            --accent-color: #ff4757; /* Brighter Red for Stop on Dark */
            --text-color: #f0f0f0;
            --card-bg: rgba(40, 40, 40, 0.6);
            --shadow-light: #2c2c2c;
            --shadow-dark: #141414;
            --thumb-color: var(--main-color);
            --track-color: rgba(255, 255, 255, 0.1);
        }

        body {
            font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
            background-color: var(--bg-color);
            color: var(--text-color);
            margin: 0;
            padding: 20px;
            display: flex;
            justify-content: center;
            align-items: center;
            min-height: 100vh;
            transition: background-color 0.3s ease, color 0.3s ease;
            user-select: none;
            -webkit-user-select: none;
        }

        .container {
            width: 100%;
            max-width: 400px;
            background: var(--card-bg);
            backdrop-filter: blur(15px);
            -webkit-backdrop-filter: blur(15px);
            border-radius: 20px;
            padding: 25px;
            box-shadow: 8px 8px 16px var(--shadow-dark), -8px -8px 16px var(--shadow-light);
            border: 1px solid rgba(255, 255, 255, 0.1);
        }
        
        h2.title {
            text-align: center;
            color: var(--main-color);
            margin-top: 0;
            margin-bottom: 25px;
            font-weight: 600;
        }

        .control-group { margin-bottom: 25px; }
        .control-group:last-child { margin-bottom: 0; }
        
        .d-pad {
            display: grid;
            grid-template-columns: 1fr 1fr 1fr;
            grid-template-rows: 1fr 1fr 1fr;
            gap: 15px;
            width: 80%;
            margin: 0 auto;
        }
        
        .d-pad-button {
            background: var(--bg-color);
            border: none;
            border-radius: 15px;
            box-shadow: 4px 4px 8px var(--shadow-dark), -4px -4px 8px var(--shadow-light);
            cursor: pointer;
            aspect-ratio: 1 / 1;
            display: flex;
            justify-content: center;
            align-items: center;
            transition: all 0.1s ease-in-out;
        }
        
        .d-pad-button:active {
            box-shadow: inset 4px 4px 8px var(--shadow-dark), inset -4px -4px 8px var(--shadow-light);
            transform: scale(0.95);
        }
        
        .d-pad-button svg {
            width: 60%;
            height: 60%;
            fill: var(--main-color);
        }

        #btn-up { grid-column: 2; grid-row: 1; }
        #btn-left { grid-column: 1; grid-row: 2; }
        #btn-stop { grid-column: 2; grid-row: 2; }
        #btn-right { grid-column: 3; grid-row: 2; }
        #btn-down { grid-column: 2; grid-row: 3; }
        
        #btn-stop svg { fill: var(--accent-color); }

        .slider-group { display: flex; flex-direction: column; }
        
        .slider-label {
            margin-bottom: 10px;
            font-weight: 500;
            color: var(--text-color);
        }
        
        input[type="range"] {
            -webkit-appearance: none;
            appearance: none;
            width: 100%;
            height: 10px;
            background: var(--track-color);
            border-radius: 5px;
            outline: none;
            box-shadow: inset 1px 1px 2px var(--shadow-dark);
        }

        input[type="range"]::-webkit-slider-thumb {
            -webkit-appearance: none; appearance: none;
            width: 24px; height: 24px; border-radius: 50%;
            background: var(--thumb-color); cursor: pointer;
            border: 3px solid var(--bg-color);
            box-shadow: 2px 2px 4px var(--shadow-dark), -2px -2px 4px var(--shadow-light);
            transition: transform 0.2s ease;
        }
        
        input[type="range"]::-webkit-slider-thumb:hover { transform: scale(1.1); }
        input[type="range"]::-moz-range-thumb {
            width: 24px; height: 24px; border-radius: 50%;
            background: var(--thumb-color); cursor: pointer;
            border: 3px solid var(--bg-color);
            box-shadow: 2px 2px 4px var(--shadow-dark), -2px -2px 4px var(--shadow-light);
        }
        
        .footer-controls {
            margin-top: 25px;
            display: flex;
            justify-content: center;
            align-items: center;
        }
        .theme-switch-checkbox { height: 0; width: 0; visibility: hidden; }
        .theme-switch-label {
            cursor: pointer; width: 50px; height: 26px; background: var(--shadow-dark);
            display: block; border-radius: 26px; position: relative;
        }
        .theme-switch-label:after {
            content: ''; position: absolute; top: 3px; left: 3px; width: 20px; height: 20px;
            background: var(--bg-color); border-radius: 20px; transition: 0.3s;
        }
        .theme-switch-checkbox:checked + .theme-switch-label:after {
            left: calc(100% - 3px); transform: translateX(-100%);
        }
    </style>
</head>
<body>
    <div class="container">
        <h2 class="title">ESP32 Rover Control</h2>

        <div class="control-group">
            <div class="d-pad">
                <button id="btn-up" class="d-pad-button" ontouchstart='sendButtonInput("MoveCar","1")' ontouchend='sendButtonInput("MoveCar","0")'>
                    <svg viewBox="0 0 24 24"><path d="M12 4L4 12h16L12 4z"/></svg>
                </button>
                <button id="btn-left" class="d-pad-button" ontouchstart='sendButtonInput("MoveCar","3")' ontouchend='sendButtonInput("MoveCar","0")'>
                    <svg viewBox="0 0 24 24"><path d="M12 4l-8 8 8 8V4z"/></svg>
                </button>
                <button id="btn-stop" class="d-pad-button" ontouchstart='sendButtonInput("MoveCar","0")'>
                    <svg viewBox="0 0 24 24"><path d="M6 6h12v12H6z"/></svg>
                </button>
                <button id="btn-right" class="d-pad-button" ontouchstart='sendButtonInput("MoveCar","4")' ontouchend='sendButtonInput("MoveCar","0")'>
                    <svg viewBox="0 0 24 24"><path d="M12 4l8 8-8 8V4z"/></svg>
                </button>
                <button id="btn-down" class="d-pad-button" ontouchstart='sendButtonInput("MoveCar","2")' ontouchend='sendButtonInput("MoveCar","0")'>
                     <svg viewBox="0 0 24 24"><path d="M12 20l8-8H4l8 8z"/></svg>
                </button>
            </div>
        </div>

        <div class="control-group">
            <div class="slider-group">
                <label for="Speed" class="slider-label">Speed</label>
                <input type="range" min="0" max="255" value="150" class="slider" id="Speed" oninput='sendButtonInput("Speed",value)'>
            </div>
        </div>

        <div class="control-group">
            <div class="slider-group">
                <label for="Pan" class="slider-label">Pan</label>
                <input type="range" min="0" max="180" value="90" class="slider" id="Pan" oninput='sendButtonInput("Pan",value)'>
            </div>
            <div class="slider-group" style="margin-top: 15px;">
                <label for="Tilt" class="slider-label">Tilt</label>
                <input type="range" min="0" max="180" value="90" class="slider" id="Tilt" oninput='sendButtonInput("Tilt",value)'>
            </div>
        </div>
        
        <div class="footer-controls">
            <div class="theme-switch">
                <input class="theme-switch-checkbox" type="checkbox" id="theme-switch">
                <label class="theme-switch-label" for="theme-switch"></label>
            </div>
        </div>
    </div>

    <script>
        var wsUrl = "ws://" + window.location.hostname + ":81/";
        var websocket;

        const themeSwitch = document.getElementById('theme-switch');
        
        function set_theme(theme_name) {
            document.documentElement.setAttribute('data-theme', theme_name);
            localStorage.setItem('theme', theme_name);
            themeSwitch.checked = (theme_name === 'dark');
        }

        themeSwitch.addEventListener('change', function (e) {
            set_theme(e.target.checked ? 'dark' : 'light');
        });

        const currentTheme = localStorage.getItem('theme') || 'dark';
        set_theme(currentTheme);

        function initWebSocket() {
            websocket = new WebSocket(wsUrl);
            websocket.onopen = function (event) {
                console.log("WebSocket connected.");
                sendButtonInput("Speed", document.getElementById("Speed").value);
                sendButtonInput("Pan", document.getElementById("Pan").value);
                sendButtonInput("Tilt", document.getElementById("Tilt").value);
            };
            websocket.onclose = function (event) { 
                console.log("WebSocket disconnected. Retrying...");
                setTimeout(initWebSocket, 2000); 
            };
            websocket.onerror = function(error) { console.error("WebSocket Error: ", error); };
        }

        function sendButtonInput(key, value) {
            if (websocket && websocket.readyState === WebSocket.OPEN) {
                var data = key + "," + value;
                websocket.send(data);
            }
        }

        window.onload = initWebSocket;
        document.addEventListener('touchend', function(event) {
            event.preventDefault();
        }, { passive: false });
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
      std::string data((char*)payload, length);
      
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

    default:
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
  webSocket.loop();
  server.handleClient();
}
