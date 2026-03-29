#ifndef DOOR_H
#define DOOR_H

/*
 * door.h — Door Lock Control & Feedback
 * Dieu khien khoa cua K01-12V va doc feedback trang thai.
 */

// ===== Door Lock State =====
bool doorUnlockActive = false;
unsigned long doorUnlockStartMs = 0;

// ===== Mo khoa cua (kich hoat relay) =====
void triggerUnlockDoor() {
  digitalWrite(LOCK_RELAY_PIN, HIGH);
  doorUnlockActive = true;
  doorUnlockStartMs = millis();
}

// ===== Tu dong khoa lai sau thoi gian mo =====
void handleUnlockDoor() {
  if (!doorUnlockActive) return;
  if (millis() - doorUnlockStartMs >= DOOR_UNLOCK_MS) {
    digitalWrite(LOCK_RELAY_PIN, LOW);
    doorUnlockActive = false;
  }
}

// ===== Doc feedback khoa cua (log Serial) =====
// TODO: Mo rong logic xu ly feedback theo yeu cau
void handleDoorFeedback() {
  static bool lastOpenFb  = HIGH;
  static bool lastCloseFb = HIGH;

  bool openFb  = digitalRead(DOOR_FB_OPEN_PIN);
  bool closeFb = digitalRead(DOOR_FB_CLOSE_PIN);

  if (openFb != lastOpenFb) {
    Serial.print("[DOOR] Feedback OPEN: ");
    Serial.println(openFb == LOW ? "ACTIVE" : "INACTIVE");
    lastOpenFb = openFb;
  }

  if (closeFb != lastCloseFb) {
    Serial.print("[DOOR] Feedback CLOSE: ");
    Serial.println(closeFb == LOW ? "ACTIVE" : "INACTIVE");
    lastCloseFb = closeFb;
  }
}

#endif
