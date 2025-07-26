#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <iostream>
#include <sstream>
#include <ESP32Servo.h>

#define PAN_PIN 14
#define TILT_PIN 15

Servo panServo;
Servo tiltServo;

struct MOTOR_PINS
{
  int pinEn;
  int pinIN1;
  int pinIN2;
};

// --- PIN CORRECTIONS ---
// - Right Motor: Changed pin 12 (strapping pin) to 23.
// - Left Motor: Changed pins 16 (RX2) and 17 (TX2) to 18 and 19.
std::vector<MOTOR_PINS> motorPins =
{
  {2, 23, 13}, //RIGHT_MOTOR Pins (EnA, IN1, IN2)
  {2, 18, 19},  //LEFT_MOTOR  Pins (EnB, IN3, IN4)
};

#define UP 1
#define DOWN 2
#define LEFT 3
#define RIGHT 4
#define STOP 0

#define RIGHT_MOTOR 0
#define LEFT_MOTOR 1

#define FORWARD 1
#define BACKWARD -1

const int PWMFreq = 1000; /* 1 KHz */
const int PWMResolution = 8;
const int PWMSpeedChannel = 2;

// WiFi network credentials - change these to match your network
const char* ssid     = "sim";
const char* password = "simple12";

// Maximum connection attempts before resetting
const int maxConnectionAttempts = 20;

AsyncWebServer server(80);
AsyncWebSocket wsCarInput("/CarInput");

// --- FINAL HTML WITH STOP BUTTON AND THEME TOGGLE ---
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
        var webSocketCarInputUrl = "ws://" + window.location.hostname + "/CarInput";
        var websocketCarInput;

        const themeSwitch = document.getElementById('theme-switch');
        
        function set_theme(theme_name) {
            document.documentElement.setAttribute('data-theme', theme_name);
            localStorage.setItem('theme', theme_name);
            themeSwitch.checked = (theme_name === 'dark');
        }

        themeSwitch.addEventListener('change', function (e) {
            set_theme(e.target.checked ? 'dark' : 'light');
        });

        const currentTheme = localStorage.getItem('theme') || 'dark'; // Default to dark theme
        set_theme(currentTheme);

        function initCarInputWebSocket() {
            websocketCarInput = new WebSocket(webSocketCarInputUrl);
            websocketCarInput.onopen = function (event) {
                console.log("WebSocket connected.");
                sendButtonInput("Speed", document.getElementById("Speed").value);
                sendButtonInput("Pan", document.getElementById("Pan").value);
                sendButtonInput("Tilt", document.getElementById("Tilt").value);
            };
            websocketCarInput.onclose = function (event) { 
                console.log("WebSocket disconnected. Retrying...");
                setTimeout(initCarInputWebSocket, 2000); 
            };
            websocketCarInput.onerror = function(error) { console.error("WebSocket Error: ", error); };
        }

        function sendButtonInput(key, value) {
            if (websocketCarInput && websocketCarInput.readyState === WebSocket.OPEN) {
                var data = key + "," + value;
                websocketCarInput.send(data);
            }
        }

        window.onload = initCarInputWebSocket;
        document.addEventListener('touchend', function(event) {
            event.preventDefault();
        }, { passive: false });
    </script>
</body>
</html>
)HTMLHOMEPAGE";


void rotateMotor(int motorNumber, int motorDirection)
{
  if (motorDirection == FORWARD)
  {
    digitalWrite(motorPins[motorNumber].pinIN1, HIGH);
    digitalWrite(motorPins[motorNumber].pinIN2, LOW);
  }
  else if (motorDirection == BACKWARD)
  {
    digitalWrite(motorPins[motorNumber].pinIN1, LOW);
    digitalWrite(motorPins[motorNumber].pinIN2, HIGH);
  }
  else
  {
    digitalWrite(motorPins[motorNumber].pinIN1, LOW);
    digitalWrite(motorPins[motorNumber].pinIN2, LOW);
  }
}

void moveCar(int inputValue)
{
  Serial.printf("Got value as %d\n", inputValue);
  switch(inputValue)
  {

    case UP:
      rotateMotor(RIGHT_MOTOR, FORWARD);
      rotateMotor(LEFT_MOTOR, FORWARD);
      break;

    case DOWN:
      rotateMotor(RIGHT_MOTOR, BACKWARD);
      rotateMotor(LEFT_MOTOR, BACKWARD);
      break;

    case LEFT:
      rotateMotor(RIGHT_MOTOR, FORWARD);
      rotateMotor(LEFT_MOTOR, BACKWARD);
      break;

    case RIGHT:
      rotateMotor(RIGHT_MOTOR, BACKWARD);
      rotateMotor(LEFT_MOTOR, FORWARD);
      break;

    case STOP:
      rotateMotor(RIGHT_MOTOR, STOP);
      rotateMotor(LEFT_MOTOR, STOP);
      break;

    default:
      rotateMotor(RIGHT_MOTOR, STOP);
      rotateMotor(LEFT_MOTOR, STOP);
      break;
  }
}

void handleRoot(AsyncWebServerRequest *request)
{
  request->send_P(200, "text/html", htmlHomePage);
}

void handleNotFound(AsyncWebServerRequest *request)
{
    request->send(404, "text/plain", "File Not Found");
}

void onCarInputWebSocketEvent(AsyncWebSocket *server,
                      AsyncWebSocketClient *client,
                      AwsEventType type,
                      void *arg,
                      uint8_t *data,
                      size_t len)
{
  switch (type)
  {
    case WS_EVT_CONNECT:
      Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("WebSocket client #%u disconnected\n", client->id());
      moveCar(0);
      panServo.write(90);
      tiltServo.write(90);
      break;
    case WS_EVT_DATA:
      AwsFrameInfo *info;
      info = (AwsFrameInfo*)arg;
      if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT)
      {
        std::string myData = "";
        myData.assign((char *)data, len);
        std::istringstream ss(myData);
        std::string key, value;
        std::getline(ss, key, ',');
        std::getline(ss, value, ',');
        Serial.printf("Key [%s] Value[%s]\n", key.c_str(), value.c_str());
        int valueInt = atoi(value.c_str());
        if (key == "MoveCar")
        {
          moveCar(valueInt);
        }
        else if (key == "Speed")
        {
          ledcWrite(PWMSpeedChannel, valueInt);
        }
        else if (key == "Pan")
        {
          panServo.write(valueInt);
        }
        else if (key == "Tilt")
        {
          tiltServo.write(valueInt);
        }
      }
      break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
    default:
      break;
  }
}

void setUpPinModes()
{
  panServo.attach(PAN_PIN);
  tiltServo.attach(TILT_PIN);

  //Set up PWM
  ledcSetup(PWMSpeedChannel, PWMFreq, PWMResolution);

  for (size_t i = 0; i < motorPins.size(); i++)
  {
    pinMode(motorPins[i].pinEn, OUTPUT);
    pinMode(motorPins[i].pinIN1, OUTPUT);
    pinMode(motorPins[i].pinIN2, OUTPUT);
    /* Attach the PWM Channel to the motor enb Pin */
    ledcAttachPin(motorPins[i].pinEn, PWMSpeedChannel);
  }
  moveCar(STOP);
}


void setup(void)
{
  setUpPinModes();
  Serial.begin(115200);

  // Connect to WiFi network in station mode
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  // Try to connect to WiFi
  int connectionAttempts = 0;
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED && connectionAttempts < maxConnectionAttempts) {
    delay(500);
    Serial.print(".");
    connectionAttempts++;
  }

  // Check if connection was successful
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Failed to connect to WiFi. Please check your credentials.");
    // Optional: You could restart the ESP or implement a fallback to AP mode
    ESP.restart();
  }

  // If connected, print the IP address
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.print("MAC address: ");
  Serial.println(WiFi.macAddress());

  server.on("/", HTTP_GET, handleRoot);
  server.onNotFound(handleNotFound);

  wsCarInput.onEvent(onCarInputWebSocketEvent);
  server.addHandler(&wsCarInput);

  server.begin();
  Serial.println("HTTP server started");
}


void loop()
{
  // Check WiFi connection status and reconnect if needed
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi connection lost. Attempting to reconnect...");
    WiFi.reconnect();
    
    // Wait for reconnection
    int reconnectAttempts = 0;
    while (WiFi.status() != WL_CONNECTED && reconnectAttempts < maxConnectionAttempts) {
      delay(500);
      Serial.print(".");
      reconnectAttempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nWiFi reconnected");
      Serial.print("IP address: ");
      Serial.println(WiFi.localIP());
    }
  }
  
  wsCarInput.cleanupClients();
}
