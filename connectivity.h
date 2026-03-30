#include "Client.h"
#ifndef CONNECTIVITY_H
#define CONNECTIVITY_H

/*
 * connectivity.h — WiFi & MQTT
 * Ket noi WiFi, MQTT, xử lý message đến, đổi mật khẩu qua MQTT.
 */

// ===== Connection State =====
unsigned long lastMqttRetryMs = 0;
unsigned long connectedAt     = 0;
bool apScheduledToClose       = false;
bool mqttFirstConnect         = true;   // flag: chua connect MQTT lan nao
bool passwordReceived         = false;  // flag: da nhan password tu server

// ===== WiFi Config State (shared with web_api.h) =====
String wifiSSID         = "";
String wifiPASS         = "";
String connectionStatus = "idle";  // idle | connecting | connected | failed
unsigned long connectStartTime = 0;

// ===== LCD Status Helpers =====
void lcdShowStatus(const char* line0, const char* line1) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(line0);
  lcd.setCursor(0, 1);
  lcd.print(line1);
}

// ===== Kết nối & giữ MQTT =====
void serviceMQTT() {
  if (WiFi.status() != WL_CONNECTED) return;
  if (client.connected()) return;

  unsigned long now = millis();
  if (now - lastMqttRetryMs < MQTT_RETRY_MS) return;
  lastMqttRetryMs = now;

  Serial.print("[MQTT] Connecting... ");
  lcdShowStatus("MQTT", "Connecting...");

  if (client.connect(DEVICE_ID)) {
    Serial.println("OK");
    lcdShowStatus("MQTT", "Connected!");

    client.subscribe(TOPIC_LIGHT);
    client.subscribe(TOPIC_DOOR);
    client.subscribe(TOPIC_ALARM);
    client.subscribe(TOPIC_STATUS);
    client.subscribe(TOPIC_GET_DOOR_PASSWORD);
    client.subscribe(TOPIC_CURTAIN);

    // Moi lan ket noi MQTT -> lay mat khau moi nhat
    client.publish(TOPIC_STATUS, "GET_DOOR_PASSWORD");
    Serial.println("[MQTT] Published GET_DOOR_PASSWORD");

    if (mqttFirstConnect) {
      lcdShowStatus("Password", "Requesting...");
      mqttFirstConnect = false;
    }
  } else {
    Serial.println("FAILED");
    lcdShowStatus("MQTT", "Failed!");
  }
}

// ===== MQTT Callback: xử lý khi có message đến =====
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String message;
  message.reserve(length);
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  Serial.printf("[MQTT] %s -> %s\n", topic, message.c_str());

  // --- Control light ---
  if (strcmp(topic, TOPIC_LIGHT) == 0) {
    if      (message == "ON_1")  digitalWrite(LED_PIN_1, HIGH);
    else if (message == "OFF_1") digitalWrite(LED_PIN_1, LOW);
    else if (message == "ON_2")  digitalWrite(LED_PIN_2, HIGH);
    else if (message == "OFF_2") digitalWrite(LED_PIN_2, LOW);
    return;
  }

  // --- Control door ---
  if (strcmp(topic, TOPIC_DOOR) == 0) {
    if (message == "ON_1") triggerUnlockDoor();
    return;
  }

  // --- Fire alarm test ---
  if (strcmp(topic, TOPIC_ALARM) == 0) {
    if (message == "ON_1") {
      fireDetected = true;
    } else if (message == "OFF_1") {
      fireDetected = false;
      firstFireState = true;
      stopAlarmOutputs();
    }
    return;
  }

  // --- Control curtain ---
  if (strcmp(topic, TOPIC_CURTAIN) == 0) {
    if (message == "ON_1") {
      curtainCmdClose = false;
      curtainCmdOpen  = true;
    } else if (message == "OFF_1") {
      curtainCmdOpen  = false;
      curtainCmdClose = true;
    }
    return;
  }

  if (strcmp(topic, TOPIC_STATUS) == 0) {
    if (message == "ARE_YOU_OK") {
      client.publish(TOPIC_STATUS, "ONLINE");
    }
    return;
  }

  // --- Change door password ---
  if (strcmp(topic, TOPIC_GET_DOOR_PASSWORD) == 0) {
    if (message.length() == PASSWORD_LEN) {
      // Dat mat khau moi
      strncpy(doorPassword, message.c_str(), PASSWORD_LEN);
      doorPassword[PASSWORD_LEN] = '\0';
      Serial.printf("[DOOR] Password updated: %s\n", doorPassword);
      client.publish(TOPIC_STATUS, "PWD_UPDATED");

      // Hien thi trang thai nhan password len LCD
      if (!passwordReceived) {
        passwordReceived = true;
        lcdShowStatus("Password", "Received OK!");
        Serial.println("[LCD] Password received, showing status");
      }
    } else {
      Serial.printf("[DOOR] Invalid password length: %d\n", message.length());
    }
    return;
  }
}

// ===== Xu ly WiFi async (tu web config) =====
void handleWiFiConnection() {
  static bool wifiStatusShown = false;

  if (connectionStatus == "connecting") {
    if (!wifiStatusShown) {
      lcdShowStatus("WiFi", "Connecting...");
      wifiStatusShown = true;
    }
    if (WiFi.status() == WL_CONNECTED) {
      connectionStatus = "connected";
      connectedAt = millis();
      apScheduledToClose = true;
      wifiStatusShown = false;
      lcdShowStatus("WiFi", "Connected!");
      Serial.print("[WIFI] Connected! IP: ");
      Serial.println(WiFi.localIP());
    }
  }

  // Tat AP sau 5s khi da ket noi WiFi Station
  if (apScheduledToClose && millis() - connectedAt > 5000) {
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
    apScheduledToClose = false;
    Serial.println("[WIFI] AP closed, switched to STA mode");
  }
}

#endif
