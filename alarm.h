#ifndef ALARM_H
#define ALARM_H

/*
 * alarm.h — Fire Alarm System
 * Phat hien lua, kich hoat coi + den SOS, gui canh bao MQTT.
 */

// ===== Alarm State =====
bool fireDetected  = false;
bool firstFireState = true;
unsigned long lastAlarmMs = 0;

// ===== Tat dau ra bao dong =====
void stopAlarmOutputs() {
  digitalWrite(LED_SOS_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);
}

// ===== Cam bien lua: doc trang thai =====
void handleFlameSensor() {
  if (digitalRead(FLAME_SENSOR) == LOW) {
    fireDetected = true;
  }
}

// ===== Xu ly bao dong: nhap nhay LED + buzzer =====
void handleAlarm() {
  if (!fireDetected) {
    stopAlarmOutputs();
    return;
  }

  unsigned long now = millis();
  if (now - lastAlarmMs < ALARM_INTERVAL_MS) return;
  lastAlarmMs = now;

  digitalWrite(LED_SOS_PIN, !digitalRead(LED_SOS_PIN));
  digitalWrite(BUZZER_PIN, !digitalRead(BUZZER_PIN));

  if (firstFireState) {
    client.publish(TOPIC_STATUS, "FIRE");
    firstFireState = false;
  }
}

// ===== Nut nhan tat bao dong =====
void handleButtonStopAlarm() {
  static bool lastState = HIGH;
  bool currentState = digitalRead(BUTTON_STOP_ALARM_PIN);

  if (lastState == HIGH && currentState == LOW) {
    fireDetected = false;
    firstFireState = true;
    stopAlarmOutputs();
    client.publish(TOPIC_STATUS, "FIRE_ACK");
  }

  lastState = currentState;
}

#endif
