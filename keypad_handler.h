#ifndef KEYPAD_HANDLER_H
#define KEYPAD_HANDLER_H

/*
 * keypad_handler.h — Keypad Password Entry
 * Xu ly nhap mat khau ban phim de mo khoa cua.
 * Mat khau co the doi qua MQTT (xem connectivity.h).
 */

// ===== Password (co the doi qua MQTT) =====
char doorPassword[PASSWORD_LEN + 1];

// ===== Keypad State =====
char inputPwd[PASSWORD_LEN + 1] = {0};
byte pwdIndex   = 0;
byte wrongCount = 0;
unsigned long lockUntilMs           = 0;
unsigned long keypadFeedbackUntilMs = 0;

// ===== Khoi tao password mac dinh =====
void initKeypadPassword() {
  strncpy(doorPassword, DEFAULT_DOOR_PASSWORD, PASSWORD_LEN);
  doorPassword[PASSWORD_LEN] = '\0';
}

// ===== Reset input =====
void resetPwd() {
  pwdIndex = 0;
  memset(inputPwd, 0, sizeof(inputPwd));
}

// ===== Hien thi mask ***___ =====
void showMask() {
  char line[PASSWORD_LEN + 1] = "______";
  for (byte i = 0; i < pwdIndex; i++) line[i] = '*';
  lcdLine(1, line);
}

// ===== Hien thi prompt nhap mat khau =====
void showPrompt() {
  lcdLine(0, "Enter Code");
  showMask();
}

// ===== Xu ly nhap phim =====
void handleKeypad() {
  unsigned long now = millis();

  // Dang hien thi feedback (dung/sai) -> cho
  if (now < keypadFeedbackUntilMs) return;

  // Dang bi khoa -> hien thi dem nguoc
  if (lockUntilMs > now) {
    static int lastSec = -1;
    int sec = (lockUntilMs - now + 999) / 1000;
    if (sec != lastSec) {
      lastSec = sec;
      char msg[17];
      snprintf(msg, sizeof(msg), "Wait %2ds", sec);
      lcdLine(0, "LOCKED");
      lcdLine(1, msg);
    }
    return;
  }

  // Vua het khoa -> show prompt lai
  if (lockUntilMs != 0) {
    lockUntilMs = 0;
    showPrompt();
  }

  char key = keypad.getKey();
  if (!key) return;

  // '*' = xoa input
  if (key == '*') {
    resetPwd();
    showPrompt();
    return;
  }

  // Chi nhan so 0-9
  if (key < '0' || key > '9' || pwdIndex >= PASSWORD_LEN) return;

  inputPwd[pwdIndex++] = key;
  showMask();

  // Chua du ky tu -> tiep tuc nhap
  if (pwdIndex < PASSWORD_LEN) return;

  inputPwd[PASSWORD_LEN] = '\0';

  // So sanh password
  if (strcmp(inputPwd, doorPassword) == 0) {
    lcdLine(0, "ACCESS GRANTED");
    lcdLine(1, "Door Open");
    triggerUnlockDoor();
    wrongCount = 0;
    keypadFeedbackUntilMs = now + UI_FEEDBACK_MS;
  } else {
    wrongCount++;
    int left = MAX_WRONG_ATTEMPTS - wrongCount;

    if (left <= 0) {
      lockUntilMs = now + LOCK_DURATION_MS;
      wrongCount = 0;
      lcdLine(0, "LOCKED");
      lcdLine(1, "Wait 30s");
    } else {
      char msg[17];
      snprintf(msg, sizeof(msg), "Try left: %d", left);
      lcdLine(0, "Wrong password");
      lcdLine(1, msg);
    }
    keypadFeedbackUntilMs = now + UI_FEEDBACK_MS;
  }

  resetPwd();
  if (lockUntilMs == 0 && now >= keypadFeedbackUntilMs) {
    showPrompt();
  }
}

// ===== Sau khi het feedback -> hien thi lai prompt =====
void handleKeypadPostFeedback() {
  if (keypadFeedbackUntilMs == 0) return;
  if (millis() < keypadFeedbackUntilMs) return;

  keypadFeedbackUntilMs = 0;
  if (lockUntilMs == 0) showPrompt();
}

#endif
