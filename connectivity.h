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

// ===== WiFi Config State (shared with web_api.h) =====
String wifiSSID         = "";
String wifiPASS         = "";
String connectionStatus = "idle";  // idle | connecting | connected | failed
unsigned long connectStartTime = 0;

// ===== Kết nối & giữ MQTT =====
void serviceMQTT() {
  if (WiFi.status() != WL_CONNECTED) return;
  if (client.connected()) return;

  unsigned long now = millis();
  if (now - lastMqttRetryMs < MQTT_RETRY_MS) return;
  lastMqttRetryMs = now;

  Serial.print("[MQTT] Connecting... ");
  if (client.connect(DEVICE_ID)) {
    Serial.println("OK");
    client.subscribe(TOPIC_LIGHT);
    client.subscribe(TOPIC_DOOR);
    client.subscribe(TOPIC_ALARM);
    client.subscribe(TOPIC_STATUS);
    client.subscribe(TOPIC_GET_DOOR_PASSWORD);
    client.subscribe(TOPIC_CURTAIN);
  } else {
    Serial.println("FAILED");
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

  // --- Change door password ---
  if (strcmp(topic, TOPIC_GET_DOOR_PASSWORD) == 0) {
    if (message == "GET") {
      // Gui lai mat khau hien tai
      client.publish(TOPIC_STATUS, doorPassword);
    } else if (message.length() == PASSWORD_LEN) {
      // Dat mat khau moi
      strncpy(doorPassword, message.c_str(), PASSWORD_LEN);
      doorPassword[PASSWORD_LEN] = '\0';
      Serial.printf("[DOOR] Password updated: %s\n", doorPassword);
      client.publish(TOPIC_STATUS, "PWD_UPDATED");
    } else {
      Serial.printf("[DOOR] Invalid password length: %d\n", message.length());
    }
    return;
  }
}

// ===== Xu ly WiFi async (tu web config) =====
void handleWiFiConnection() {
  if (connectionStatus == "connecting") {
    if (WiFi.status() == WL_CONNECTED) {
      connectionStatus = "connected";
      connectedAt = millis();
      apScheduledToClose = true;
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
