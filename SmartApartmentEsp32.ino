#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ESP32Servo.h>
#include <Keypad.h>
#include <Keypad_I2C.h>
#include "config.h"

// ---------- Config ----------
const byte I2C_ADDR = 0x20;
const byte ROWS = 4;
const byte COLS = 3;

const uint8_t LCD_COLS = 16;
const uint8_t PASS_LEN = 6;
const uint8_t MAX_WRONG = 5;

const unsigned long SCREEN_TIMEOUT_MS = 60000UL;      // 1 minute
const unsigned long MQTT_RETRY_MS = 2000UL;
const unsigned long LOCK_TIME_MS = 30000UL;
const unsigned long MSG_HOLD_MS = 1200UL;
const unsigned long FLOW_SAMPLE_MS = 1000UL;
const unsigned long ALARM_INTERVAL_MS = 200UL;
const unsigned long SERVO_RUN_MS = 2000UL;
const unsigned long GET_PASS_INTERVAL_MS = 5000UL;
const unsigned long USAGE_INTERVAL_MS = 10000UL;

// ---------- Keypad ----------
char keys[ROWS][COLS] = {
  { '1', '2', '3' },
  { '4', '5', '6' },
  { '7', '8', '9' },
  { '*', '0', '#' }
};

byte rowPins[ROWS] = { 0, 1, 2, 3 };
byte colPins[COLS] = { 4, 5, 6 };
Keypad_I2C keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS, I2C_ADDR);

// ---------- Devices ----------
WiFiClient espClient;
PubSubClient client(espClient);

LiquidCrystal_I2C lcdMain(0x26, 16, 2);   // water/electric
LiquidCrystal_I2C lcdDoor(0x27, 16, 2);   // password
Servo curtainServo;

// ---------- State ----------
bool fireDetected = false;
bool firstFireState = true;
int displayMode = MODE_WATER;

volatile uint32_t pulseCount = 0;
float waterFlowRate = 0.0f;
float waterTotal = 0.0f;
float electricPower = 0.0f;
float electricEnergy = 0.0f;

bool curtainOpenReq = false;
bool curtainCloseReq = false;
bool servoRunning = false;

String doorPassword = "";
String inputPassword = "";
uint8_t wrongCount = 0;
bool lockInput = false;
unsigned long lockUntilMs = 0;

String doorMsg1 = "";
String doorMsg2 = "";
unsigned long doorMsgUntilMs = 0;

bool screensOn = false;
unsigned long lastUserActionMs = 0;

// ---------- Timers ----------
unsigned long mqttTryMs = 0;
unsigned long alarmTimer = 0;
unsigned long flowTimer = 0;
unsigned long servoTimer = 0;
unsigned long doorRequestTimer = 0;
unsigned long usageTimer = 0;

// ---------- Interrupt ----------
void IRAM_ATTR pulseCounter() {
  pulseCount++;
}

// ---------- Helpers ----------
void printLine(LiquidCrystal_I2C& lcd, uint8_t row, const String& text) {
  char line[LCD_COLS + 1];
  snprintf(line, sizeof(line), "%-16.16s", text.c_str());
  lcd.setCursor(0, row);
  lcd.print(line);
}

String stars(uint8_t n) {
  String s = "";
  for (uint8_t i = 0; i < n; i++) s += "*";
  return s;
}

void setScreens(bool on) {
  if (screensOn == on) return;
  screensOn = on;

  if (on) {
    lcdMain.backlight();
    lcdDoor.backlight();
  } else {
    lcdMain.noBacklight();
    lcdDoor.noBacklight();
  }
}

void onUserAction() {
  lastUserActionMs = millis();
  if (!screensOn) setScreens(true);
}

void handleScreenTimeout() {
  if (!screensOn) return;
  if (millis() - lastUserActionMs >= SCREEN_TIMEOUT_MS) {
    setScreens(false);
  }
}

void setDoorMessage(const String& line1, const String& line2 = "", unsigned long holdMs = MSG_HOLD_MS) {
  doorMsg1 = line1;
  doorMsg2 = line2;
  doorMsgUntilMs = millis() + holdMs;
}

bool isDoorMessageActive() {
  return doorMsg1.length() > 0 && (long)(doorMsgUntilMs - millis()) > 0;
}

void clearDoorMessageIfExpired() {
  if (doorMsg1.length() == 0) return;
  if ((long)(millis() - doorMsgUntilMs) >= 0) {
    doorMsg1 = "";
    doorMsg2 = "";
  }
}

// ---------- WiFi ----------
void connectWiFi() {
  Serial.print("Connecting WiFi");
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi Connected");
}

// ---------- MQTT ----------
void subscribeTopics() {
  client.subscribe(TOPIC_LIGHT);
  client.subscribe(TOPIC_DOOR);
  client.subscribe(TOPIC_ALARM);
  client.subscribe(TOPIC_CURTAIN);
  client.subscribe(TOPIC_GET_DOOR_PASSWORD);
}

void connectMQTT() {
  if (client.connected()) return;
  if (millis() - mqttTryMs < MQTT_RETRY_MS) return;

  mqttTryMs = millis();
  Serial.print("Connecting MQTT...");

  if (client.connect(DEVICE_ID)) {
    Serial.println("Connected");
    subscribeTopics();
    setDoorMessage("MQTT Connected", "", 800);
  } else {
    Serial.print("Failed rc=");
    Serial.println(client.state());
  }
}

// ---------- Alarm ----------
void stopAlarm() {
  fireDetected = false;
  firstFireState = true;
  digitalWrite(LED_SOS_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);
  client.publish(TOPIC_STATUS, "FIRE_ACK");
}

void handleAlarm() {
  if (!fireDetected) return;
  if (millis() - alarmTimer < ALARM_INTERVAL_MS) return;

  alarmTimer = millis();
  digitalWrite(LED_SOS_PIN, !digitalRead(LED_SOS_PIN));
  digitalWrite(BUZZER_PIN, !digitalRead(BUZZER_PIN));

  if (firstFireState) {
    client.publish(TOPIC_STATUS, "FIRE");
    firstFireState = false;
  }
}

void handleFlameSensor() {
  if (digitalRead(FLAME_SENSOR) == LOW) {
    fireDetected = true;
  }
}

void handleStopAlarmButton() {
  static bool lastState = HIGH;
  bool state = digitalRead(BUTTON_STOP_ALARM_PIN);

  if (lastState == HIGH && state == LOW) {
    onUserAction();
    stopAlarm();
  }
  lastState = state;
}

// ---------- Curtain ----------
void handleCurtain() {
  if (curtainOpenReq) {
    curtainServo.write(0);
    servoTimer = millis();
    servoRunning = true;
    curtainOpenReq = false;
  }

  if (curtainCloseReq) {
    curtainServo.write(180);
    servoTimer = millis();
    servoRunning = true;
    curtainCloseReq = false;
  }

  if (servoRunning && millis() - servoTimer >= SERVO_RUN_MS) {
    curtainServo.write(90);
    servoRunning = false;
  }
}

// ---------- Flow ----------
void handleFlowSensor() {
  if (millis() - flowTimer < FLOW_SAMPLE_MS) return;
  flowTimer = millis();

  noInterrupts();
  uint32_t count = pulseCount;
  pulseCount = 0;
  interrupts();

  waterFlowRate = count / 7.5f;
  waterTotal += waterFlowRate / 60.0f;
}

// ---------- Main LCD ----------
void handleScreenButton() {
  static bool lastState = HIGH;
  static unsigned long debounceMs = 0;

  bool state = digitalRead(BUTTON_SCREEN_PIN);
  if (lastState == HIGH && state == LOW && millis() - debounceMs > 200) {
    onUserAction();
    displayMode = (displayMode == MODE_WATER) ? MODE_ELECTRIC : MODE_WATER;
    debounceMs = millis();
  }
  lastState = state;
}

void updateMainLCD() {
  if (!screensOn) return;

  if (displayMode == MODE_WATER) {
    printLine(lcdMain, 0, "Flow:" + String(waterFlowRate, 1) + "L/m");
    printLine(lcdMain, 1, "Total:" + String(waterTotal, 1) + "L");
  } else {
    printLine(lcdMain, 0, "Power:" + String(electricPower, 1) + "W");
    printLine(lcdMain, 1, "Energy:" + String(electricEnergy, 2) + "kWh");
  }
}

// ---------- Door Password ----------
void submitDoorPassword() {
  if (inputPassword.length() != PASS_LEN) {
    setDoorMessage("Need 6 digits");
    return;
  }

  if (doorPassword.length() != PASS_LEN) {
    setDoorMessage("No server pass", "Wait syncing");
    return;
  }

  if (inputPassword == doorPassword) {
    client.publish(TOPIC_DOOR, "OPEN_1");
    inputPassword = "";
    wrongCount = 0;
    setDoorMessage("Door Open OK");
    return;
  }

  // Wrong: clear pass, show remaining attempts
  inputPassword = "";
  wrongCount++;

  int left = MAX_WRONG - wrongCount;
  if (left <= 0) {
    lockInput = true;
    lockUntilMs = millis() + LOCK_TIME_MS;
    setDoorMessage("Wrong Password", "Left:0");
  } else {
    setDoorMessage("Wrong Password", "Left:" + String(left));
  }
}

void handleDoorLock() {
  if (!lockInput) return;

  if ((long)(millis() - lockUntilMs) >= 0) {
    lockInput = false;
    wrongCount = 0;
    setDoorMessage("Unlocked", "Enter pass", 1000);
  }
}

void handleKeypadDoor() {
  char key = keypad.getKey();
  if (!key) return;

  onUserAction();

  if (lockInput) return;
  if (isDoorMessageActive()) return;

  if (key == '*') {
    inputPassword = "";
    return;
  }

  if (key == '#') {
    submitDoorPassword();
    return;
  }

  if (isDigit(key) && inputPassword.length() < PASS_LEN) {
    inputPassword += key;
  }
}

void updateDoorLCD() {
  if (!screensOn) return;

  if (lockInput) {
    long remain = (long)(lockUntilMs - millis());
    unsigned long remainSec = (remain <= 0) ? 0 : ((unsigned long)(remain + 999) / 1000);
    printLine(lcdDoor, 0, "Locked: " + String(remainSec) + "s");
    printLine(lcdDoor, 1, "Left:0");
    return;
  }

  if (isDoorMessageActive()) {
    printLine(lcdDoor, 0, doorMsg1);
    printLine(lcdDoor, 1, doorMsg2);
    return;
  }

  printLine(lcdDoor, 0, "Pass:" + stars(inputPassword.length()));
  printLine(lcdDoor, 1, "");
}

void requestDoorPassword() {
  if (doorPassword.length() == PASS_LEN) return;

  if (millis() - doorRequestTimer >= GET_PASS_INTERVAL_MS) {
    client.publish(TOPIC_STATUS, "GET_DOOR_PASSWORD");
    doorRequestTimer = millis();
  }
}

// ---------- MQTT callback ----------
void onLight(const char* msg) {
  if (strcmp(msg, "ON_1") == 0) digitalWrite(LED_PIN_1, HIGH);
  else if (strcmp(msg, "OFF_1") == 0) digitalWrite(LED_PIN_1, LOW);
  else if (strcmp(msg, "ON_2") == 0) digitalWrite(LED_PIN_2, HIGH);
  else if (strcmp(msg, "OFF_2") == 0) digitalWrite(LED_PIN_2, LOW);
}

void onDoor(const char* msg) {
  if (strcmp(msg, "OPEN_1") == 0) Serial.println("Door Open command");
  else if (strcmp(msg, "CLOSE_1") == 0) Serial.println("Door Close command");
}

void onAlarm(const char* msg) {
  if (strcmp(msg, "ON_1") == 0) fireDetected = true;
  else if (strcmp(msg, "OFF_1") == 0) stopAlarm();
}

void onCurtain(const char* msg) {
  if (strcmp(msg, "OPEN_1") == 0) curtainOpenReq = true;
  else if (strcmp(msg, "CLOSE_1") == 0) curtainCloseReq = true;
}

void onDoorPassword(const char* msg) {
  String digits = "";
  for (uint8_t i = 0; msg[i] != '\0'; i++) {
    if (isDigit(msg[i])) {
      digits += msg[i];
      if (digits.length() >= PASS_LEN) break;
    }
  }

  if (digits.length() == PASS_LEN) {
    doorPassword = digits;
    if (!lockInput) setDoorMessage("Password Synced", "", 900);
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  char msg[96];
  unsigned int n = (length < sizeof(msg) - 1) ? length : (sizeof(msg) - 1);
  memcpy(msg, payload, n);
  msg[n] = '\0';

  if (strcmp(topic, TOPIC_LIGHT) == 0) onLight(msg);
  else if (strcmp(topic, TOPIC_DOOR) == 0) onDoor(msg);
  else if (strcmp(topic, TOPIC_ALARM) == 0) onAlarm(msg);
  else if (strcmp(topic, TOPIC_CURTAIN) == 0) onCurtain(msg);
  else if (strcmp(topic, TOPIC_GET_DOOR_PASSWORD) == 0) onDoorPassword(msg);
}

// ---------- Usage publish ----------
void publishUsage() {
  if (millis() - usageTimer < USAGE_INTERVAL_MS) return;
  usageTimer = millis();

  char payload[120];
  snprintf(payload, sizeof(payload),
           "{\"waterTotal\":%.2f,\"waterFlow\":%.2f,\"electric\":%.2f}",
           waterTotal, waterFlowRate, electricEnergy);

  // client.publish(TOPIC_USAGE, payload);
}

// ---------- Setup / Loop ----------
void setup() {
  Serial.begin(115200);
  Wire.begin();
  keypad.begin();

  pinMode(LED_PIN_1, OUTPUT);
  pinMode(LED_PIN_2, OUTPUT);
  pinMode(LED_SOS_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  pinMode(FLAME_SENSOR, INPUT);
  pinMode(BUTTON_STOP_ALARM_PIN, INPUT_PULLUP);
  pinMode(FLOW_PIN, INPUT_PULLUP);
  pinMode(BUTTON_SCREEN_PIN, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(FLOW_PIN), pulseCounter, FALLING);

  curtainServo.setPeriodHertz(50);
  curtainServo.attach(SERVO_PIN);
  curtainServo.write(90);

  lcdMain.init();
  lcdDoor.init();
  setScreens(false);   // start with screens OFF

  connectWiFi();
  client.setServer(MQTT_SERVER, MQTT_PORT);
  client.setCallback(callback);
}

void loop() {
  connectMQTT();
  client.loop();

  clearDoorMessageIfExpired();

  // User actions first
  handleKeypadDoor();
  handleStopAlarmButton();
  handleScreenButton();

  // Main logic
  handleDoorLock();
  requestDoorPassword();
  handleCurtain();
  handleFlameSensor();
  handleAlarm();
  handleFlowSensor();

  // Display
  updateDoorLCD();
  updateMainLCD();

  publishUsage();
  handleScreenTimeout();
}