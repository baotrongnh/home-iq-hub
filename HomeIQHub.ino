/*
 * SmartApartmentEsp32New — Smart Apartment IoT Hub
 * ESP32-based home automation: lights, door lock, curtain,
 * fire alarm, water flow, power meter, keypad access.
 *
 * Module structure:
 *   config.h          -> Pin, MQTT, WiFi, constants
 *   sensors.h         -> Water flow & power meter (PZEM)
 *   display.h         -> LCD utility display
 *   alarm.h           -> Fire alarm system
 *   door.h            -> Door lock & feedback
 *   curtain.h         -> Curtain motor control
 *   keypad_handler.h  -> Keypad password entry
 *   connectivity.h    -> WiFi & MQTT
 *   web_api.h         -> Web server API
 */

// ===== Libraries =====
#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <Keypad_I2C.h>
#include <Keypad.h>
#include <LiquidCrystal_I2C.h>
#include <ESP32Servo.h>
#include <PZEM004Tv30.h>
#include <WebServer.h>
#include <ArduinoJson.h>

// ===== Configuration =====
#include "config.h"

// ===== Global Objects =====
PZEM004Tv30 pzem(Serial2, PZEM_RX_PIN, PZEM_TX_PIN);
Servo curtainServo;
WiFiClient espClient;
PubSubClient client(espClient);

LiquidCrystal_I2C lcd(0x26, 16, 2);         // LCD chinh (keypad/password)
LiquidCrystal_I2C lcdUtility(0x27, 16, 2);  // LCD tien ich (nuoc/dien)

// ===== Keypad Setup =====
const byte ROWS = 4;
const byte COLS = 3;
char keys[ROWS][COLS] = {
  { '1', '2', '3' },
  { '4', '5', '6' },
  { '7', '8', '9' },
  { '*', '0', '#' }
};
byte rowPins[ROWS] = { 0, 1, 2, 3 };
byte colPins[COLS] = { 4, 5, 6 };
Keypad_I2C keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS, 0x20);

WebServer server(80);

// ===== Modules (thu tu include quan trong!) =====
#include "sensors.h"
#include "display.h"
#include "alarm.h"
#include "door.h"
#include "curtain.h"
#include "keypad_handler.h"
#include "connectivity.h"
#include "web_api.h"

// ========================= SETUP =========================
void setup() {
  Serial.begin(115200);
  Serial.println("\n=== Smart Apartment ESP32 ===");

  // --- GPIO: Outputs ---
  pinMode(LED_PIN_1,      OUTPUT);
  pinMode(LED_PIN_2,      OUTPUT);
  pinMode(LOCK_RELAY_PIN, OUTPUT);
  pinMode(LED_SOS_PIN,    OUTPUT);
  pinMode(BUZZER_PIN,     OUTPUT);

  // --- GPIO: Inputs ---
  pinMode(FLAME_SENSOR,           INPUT);
  pinMode(FLOW_PIN,               INPUT_PULLUP);
  pinMode(BUTTON_STOP_ALARM_PIN,  INPUT);
  pinMode(BUTTON_SCREEN_PIN,      INPUT_PULLUP);
  pinMode(LIMIT_SWITCH_OPEN_PIN,  INPUT_PULLUP);
  pinMode(LIMIT_SWITCH_CLOSE_PIN, INPUT_PULLUP);
  pinMode(DOOR_FB_OPEN_PIN,       INPUT);
  pinMode(DOOR_FB_CLOSE_PIN,      INPUT);

  // --- Interrupt: Flow sensor ---
  attachInterrupt(digitalPinToInterrupt(FLOW_PIN), pulseCounter, FALLING);

  // --- Servo ---
  curtainServo.setPeriodHertz(50);
  curtainServo.attach(SERVO_PIN, 500, 2400);
  curtainServo.write(SERVO_STOP_PWM);

  // --- I2C: LCD & Keypad ---
  Wire.begin();
  lcd.init();
  lcd.backlight();
  lcd.print("WELCOME");

  lcdUtility.init();
  lcdUtility.backlight();
  lcdUtility.clear();
  lcdUtility.print("Utility Ready");

  keypad.begin();

  // --- Keypad password ---
  initKeypadPassword();

  // --- WiFi: Access Point ---
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(AP_SSID, AP_PASS);
  Serial.print("[WIFI] AP started, IP: ");
  Serial.println(WiFi.softAPIP());

  // --- Web API ---
  setupWebRoutes();

  // --- MQTT ---
  client.setServer(MQTT_SERVER, MQTT_PORT);
  client.setCallback(mqttCallback);
}

// ========================= LOOP =========================
void loop() {
  // --- Boot splash: hien 2s roi xoa (non-blocking) ---
  static bool bootDone = false;
  if (!bootDone && millis() > 2000) {
    lcd.clear();
    showPrompt();
    lcdUtility.clear();
    bootDone = true;
  }

  // --- Web server ---
  server.handleClient();

  // --- WiFi & MQTT ---
  handleWiFiConnection();
  serviceMQTT();
  client.loop();

  // --- Fire alarm ---
  handleFlameSensor();
  handleButtonStopAlarm();
  handleAlarm();

  // --- Sensors ---
  handleFlowSensor();
  handlePzemPoll();

  // --- Display ---
  handleUtilityScreenButton();
  updateUtilityLcd();

  // --- Actuators ---
  handleCurtain();
  handleUnlockDoor();
  handleDoorFeedback();

  // --- Keypad ---
  handleKeypad();
  handleKeypadPostFeedback();
}
