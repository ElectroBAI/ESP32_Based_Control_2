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

const char* htmlHomePage PROGMEM = R"HTMLHOMEPAGE(
<!DOCTYPE html>
<html>
  <head>
    <meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1, user-scalable=no">
    <title>RC Car Control</title>
    <style>
      :root {
        --primary-color: #0074D9;  /* Blue */
        --secondary-color: #FFDC00; /* Yellow */
        --text-color: #111;
        --background-color: #f9f9f9;
        --button-color: var(--primary-color);
        --button-shadow: rgba(0, 0, 0, 0.3);
        --slider-track: #d3d3d3;
        --slider-thumb: var(--secondary-color);
      }

      [data-theme="dark"] {
        --primary-color: #004e92;  /* Darker Blue */
        --secondary-color: #FFD700; /* Gold Yellow */
        --text-color: #f0f0f0;
        --background-color: #121212;
        --button-color: var(--primary-color);
        --button-shadow: rgba(0, 0, 0, 0.5);
        --slider-track: #444;
        --slider-thumb: var(--secondary-color);
      }

      body {
        background-color: var(--background-color);
        color: var(--text-color);
        font-family: Arial, sans-serif;
        transition: all 0.3s ease;
      }

      .theme-switch {
        position: fixed;
        top: 10px;
        right: 10px;
        display: flex;
        align-items: center;
        gap: 8px;
      }

      .theme-switch-checkbox {
        height: 0;
        width: 0;
        visibility: hidden;
      }

      .theme-switch-label {
        cursor: pointer;
        width: 50px;
        height: 24px;
        background: var(--primary-color);
        display: block;
        border-radius: 24px;
        position: relative;
      }

      .theme-switch-label:after {
        content: '';
        position: absolute;
        top: 3px;
        left: 3px;
        width: 18px;
        height: 18px;
        background: var(--secondary-color);
        border-radius: 18px;
        transition: 0.3s;
      }

      .theme-switch-checkbox:checked + .theme-switch-label:after {
        left: calc(100% - 3px);
        transform: translateX(-100%);
      }

      .theme-icon {
        font-size: 16px;
      }

      .arrows {
        font-size: 30px;
        color: var(--secondary-color);
      }

      td.button {
        background-color: var(--button-color);
        border-radius: 25%;
        box-shadow: 5px 5px var(--button-shadow);
        margin: 10px;
        cursor: pointer;
      }

      td.button:active {
        transform: translate(5px,5px);
        box-shadow: none;
      }

      .noselect {
        -webkit-touch-callout: none;
        -webkit-user-select: none;
        -khtml-user-select: none;
        -moz-user-select: none;
        -ms-user-select: none;
        user-select: none;
      }

      .slidecontainer {
        width: 100%;
      }

      .slider {
        -webkit-appearance: none;
        width: 100%;
        height: 15px;
        border-radius: 5px;
        background: var(--slider-track);
        outline: none;
        opacity: 0.7;
        -webkit-transition: .2s;
        transition: opacity .2s;
      }

      .slider:hover {
        opacity: 1;
      }

      .slider::-webkit-slider-thumb {
        -webkit-appearance: none;
        appearance: none;
        width: 25px;
        height: 25px;
        border-radius: 50%;
        background: var(--slider-thumb);
        cursor: pointer;
      }

      .slider::-moz-range-thumb {
        width: 25px;
        height: 25px;
        border-radius: 50%;
        background: var(--slider-thumb);
        cursor: pointer;
      }

      #mainTable {
        width: 450px;
        margin: auto;
        table-layout: fixed;
        border-spacing: 10px;
      }

      .control-label {
        text-align: left;
        font-weight: bold;
        color: var(--text-color);
      }

      .title {
        color: var(--primary-color);
        margin-bottom: 15px;
      }

      @media (max-width: 500px) {
        #mainTable {
          width: 95%;
        }
      }
    </style>
  </head>
  <body class="noselect" align="center">
    <h2 class="title">RC Car Control</h2>

    <div class="theme-switch">
      <span class="theme-icon">Light</span>
      <input class="theme-switch-checkbox" type="checkbox" id="theme-switch">
      <label class="theme-switch-label" for="theme-switch"></label>
      <span class="theme-icon">Dark</span>
    </div>

    <table id="mainTable">
      <tr>
        <td></td>
        <td class="button" ontouchstart='sendButtonInput("MoveCar","1")' ontouchend='sendButtonInput("MoveCar","0")'>
          <span class="arrows">⇧</span>
        </td>
        <td></td>
      </tr>
      <tr>
        <td class="button" ontouchstart='sendButtonInput("MoveCar","3")' ontouchend='sendButtonInput("MoveCar","0")'>
          <span class="arrows">⇦</span>
        </td>
        <td class="button"></td>
        <td class="button" ontouchstart='sendButtonInput("MoveCar","4")' ontouchend='sendButtonInput("MoveCar","0")'>
          <span class="arrows">⇨</span>
        </td>
      </tr>
      <tr>
        <td></td>
        <td class="button" ontouchstart='sendButtonInput("MoveCar","2")' ontouchend='sendButtonInput("MoveCar","0")'>
          <span class="arrows">⇩</span>
        </td>
        <td></td>
      </tr>
      <tr><td colspan="3" style="height:20px"></td></tr>
      <tr>
        <td class="control-label">Speed:</td>
        <td colspan="2">
          <div class="slidecontainer">
            <input type="range" min="0" max="255" value="150" class="slider" id="Speed" oninput='sendButtonInput("Speed",value)'>
          </div>
        </td>
      </tr>
      <tr>
        <td class="control-label">Pan:</td>
        <td colspan="2">
          <div class="slidecontainer">
            <input type="range" min="0" max="180" value="90" class="slider" id="Pan" oninput='sendButtonInput("Pan",value)'>
          </div>
        </td>
      </tr>
      <tr>
        <td class="control-label">Tilt:</td>
        <td colspan="2">
          <div class="slidecontainer">
            <input type="range" min="0" max="180" value="90" class="slider" id="Tilt" oninput='sendButtonInput("Tilt",value)'>
          </div>
        </td>
      </tr>
    </table>

    <script>
      var webSocketCarInputUrl = "ws:\/\/" + window.location.hostname + "/CarInput";
      var websocketCarInput;

      // Theme toggle functionality
      const themeSwitch = document.getElementById('theme-switch');

      // Check for saved theme preference or prefer-color-scheme
      const currentTheme = localStorage.getItem('theme') ||
        (window.matchMedia('(prefers-color-scheme: dark)').matches ? 'dark' : 'light');

      // Set initial theme
      if (currentTheme === 'dark') {
        document.documentElement.setAttribute('data-theme', 'dark');
        themeSwitch.checked = true;
      }

      // Theme switch event handler
      themeSwitch.addEventListener('change', function(e) {
        if (e.target.checked) {
          document.documentElement.setAttribute('data-theme', 'dark');
          localStorage.setItem('theme', 'dark');
        } else {
          document.documentElement.setAttribute('data-theme', 'light');
          localStorage.setItem('theme', 'light');
        }
      });

      function initCarInputWebSocket()
      {
        websocketCarInput = new WebSocket(webSocketCarInputUrl);
        websocketCarInput.onopen    = function(event)
        {
          sendButtonInput("Speed", document.getElementById("Speed").value);
          sendButtonInput("Pan", document.getElementById("Pan").value);
          sendButtonInput("Tilt", document.getElementById("Tilt").value);
        };
        websocketCarInput.onclose   = function(event){setTimeout(initCarInputWebSocket, 2000);};
        websocketCarInput.onmessage = function(event){};
      }

      function initWebSocket()
      {
        initCarInputWebSocket();
      }

      function sendButtonInput(key, value)
      {
        var data = key + "," + value;
        websocketCarInput.send(data);
      }

      window.onload = initWebSocket;
      document.getElementById("mainTable").addEventListener("touchend", function(event){
        event.preventDefault();
      });
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

  for (int i = 0; i < motorPins.size(); i++)
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
