#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ESP32Servo.h>

#include "config.h"

Servo myServo;

WiFiClient espClient;
PubSubClient client(espClient);
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ===== STATE =====
bool alarmTestMode = false;
bool fireDetected = false;
bool firstFireState = true;

// ===== FLOW SENSOR =====
volatile int pulseCount = 0;
float flowRate = 0;
float totalLiters = 0;

unsigned long lastFlowMillis = 0;
unsigned long previousMillis = 0;

const int ALARM_INTERVAL = 200;

// ================= INTERRUPT =================
void IRAM_ATTR pulseCounter() {
  pulseCount++;
}

// ================= WIFI =================
void connectWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Connecting WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("WiFi Connected");
  delay(2000);
}

// ================= MQTT =================
void connectMQTT() {

  while (!client.connected()) {
    Serial.print("Connecting MQTT...");
    if (client.connect(DEVICE_ID)) {
      Serial.println("\nMQTT connected");

      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("MQTT connected");
      delay(2000);

      client.subscribe(TOPIC_LIGHT);
      client.subscribe(TOPIC_CMD);
      client.subscribe(TOPIC_DOOR);
    } else {
      Serial.println("failed");
      delay(1000);
    }
  }
}

// ================= MQTT CALLBACK =================
void callback(char* topic, byte* payload, unsigned int length) {

  String message;

  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  Serial.println(message);

  // ===== LIGHT =====
  if (strcmp(topic, TOPIC_LIGHT) == 0) {

    if (message == "ON_1") digitalWrite(LED_PIN_1, HIGH);
    if (message == "OFF_1") digitalWrite(LED_PIN_1, LOW);

    if (message == "ON_2") digitalWrite(LED_PIN_2, HIGH);
    if (message == "OFF_2") digitalWrite(LED_PIN_2, LOW);

  }

  // ===== DOOR =====
  else if (strcmp(topic, TOPIC_DOOR) == 0) {

    if (message == "DOOR_OPEN_1") myServo.write(90);
    if (message == "DOOR_CLOSE_1") myServo.write(0);

  }

  // ===== ALARM TEST =====
  else if (strcmp(topic, TOPIC_CMD) == 0) {

    if (message == "ALARM_ON_1") {
      alarmTestMode = true;
    }

    if (message == "ALARM_OFF_1") {

      alarmTestMode = false;
      firstFireState = true;

      digitalWrite(LED_SOS_PIN, LOW);
      digitalWrite(BUZZER_PIN, LOW);
    }
  }
}

// ================= FLOW SENSOR =================
void handleFlowSensor() {

  if (millis() - lastFlowMillis < 1000) return;

  lastFlowMillis = millis();

  noInterrupts();
  int count = pulseCount;
  pulseCount = 0;
  interrupts();

  flowRate = count / 7.5;
  totalLiters += flowRate / 60.0;

  lcd.setCursor(0, 0);
  lcd.print("Flow:");
  lcd.print(flowRate, 2);
  lcd.print(" L/m  ");

  lcd.setCursor(0, 1);
  lcd.print("Total:");
  lcd.print(totalLiters, 2);
  lcd.print(" L   ");
}

// ================= FLAME SENSOR =================
void handleFlameSensor() {

  if (digitalRead(FLAME_SENSOR) == LOW) {
    fireDetected = true;
  }
}

// ================= BUTTON =================
void handleButton() {

  static bool lastState = HIGH;
  bool currentState = digitalRead(BUTTON_PIN);

  if (lastState == HIGH && currentState == LOW) {

    Serial.println("STOP ALARM");

    fireDetected = false;
    alarmTestMode = false;
    firstFireState = true;

    digitalWrite(LED_SOS_PIN, LOW);
    digitalWrite(BUZZER_PIN, LOW);

    client.publish(TOPIC_STATUS, "FIRE_ACK");
  }

  lastState = currentState;
}

// ================= ALARM =================
void handleAlarm() {

  if (!(fireDetected || alarmTestMode)) {

    digitalWrite(LED_SOS_PIN, LOW);
    digitalWrite(BUZZER_PIN, LOW);
    return;
  }

  if (millis() - previousMillis < ALARM_INTERVAL) return;

  previousMillis = millis();

  digitalWrite(LED_SOS_PIN, !digitalRead(LED_SOS_PIN));
  digitalWrite(BUZZER_PIN, !digitalRead(BUZZER_PIN));

  if (firstFireState) {

    client.publish(TOPIC_STATUS, "FIRE");
    firstFireState = false;
  }
}

// ================= SETUP =================
void setup() {

  Serial.begin(115200);

  pinMode(LED_PIN_1, OUTPUT);
  pinMode(LED_PIN_2, OUTPUT);
  pinMode(FLAME_SENSOR, INPUT);

  pinMode(LED_SOS_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  pinMode(FLOW_PIN, INPUT_PULLUP);
  pinMode(BUTTON_PIN, INPUT);

  attachInterrupt(digitalPinToInterrupt(FLOW_PIN), pulseCounter, FALLING);

  myServo.setPeriodHertz(50);
  myServo.attach(SERVO_PIN, 500, 2400);
  myServo.write(0);

  lcd.init();
  lcd.backlight();

  lcd.print("WELCOME");
  delay(2000);
  lcd.clear();

  connectWiFi();

  client.setServer(MQTT_SERVER, MQTT_PORT);
  client.setCallback(callback);
}

// ================= LOOP =================
void loop() {

  if (!client.connected()) {
    connectMQTT();
  }

  client.loop();

  handleFlameSensor();
  handleButton();
  handleAlarm();
  handleFlowSensor();
}