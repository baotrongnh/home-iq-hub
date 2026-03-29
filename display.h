#ifndef DISPLAY_H
#define DISPLAY_H

/*
 * display.h — LCD Display Handlers
 * Hien thi thong tin len LCD chinh (keypad) va LCD tien ich (nuoc/dien).
 */

// ===== Screen Enum =====
enum UtilityScreen { SCREEN_WATER = 0, SCREEN_ELECTRIC = 1 };
UtilityScreen utilityScreen = SCREEN_WATER;

// ===== State =====
bool lastToggleBtnState        = HIGH;
unsigned long lastToggleMs     = 0;
unsigned long lastUtilityLcdMs = 0;

// ===== LCD Helpers: ghi 1 dong 16 ky tu =====
void lcdLine(byte row, const char* text) {
  char buf[17];
  snprintf(buf, sizeof(buf), "%-16s", text);
  lcd.setCursor(0, row);
  lcd.print(buf);
}

void utilityLcdLine(byte row, const char* text) {
  char buf[17];
  snprintf(buf, sizeof(buf), "%-16s", text);
  lcdUtility.setCursor(0, row);
  lcdUtility.print(buf);
}

// ===== Button chuyen man hinh nuoc <-> dien =====
void handleUtilityScreenButton() {
  bool currentBtnState = digitalRead(BUTTON_SCREEN_PIN);
  unsigned long now = millis();

  if (lastToggleBtnState == HIGH && currentBtnState == LOW) {
    if (now - lastToggleMs >= TOGGLE_DEBOUNCE_MS) {
      lastToggleMs = now;
      utilityScreen = (utilityScreen == SCREEN_WATER)
                        ? SCREEN_ELECTRIC : SCREEN_WATER;
      lcdUtility.clear();
    }
  }
  lastToggleBtnState = currentBtnState;
}

// ===== Cap nhat LCD tien ich (nuoc hoac dien) =====
void updateUtilityLcd() {
  unsigned long now = millis();
  if (now - lastUtilityLcdMs < UTILITY_LCD_INTERVAL_MS) return;
  lastUtilityLcdMs = now;

  char line0[17], line1[17];

  if (utilityScreen == SCREEN_WATER) {
    snprintf(line0, sizeof(line0), "F:%5.2f L/min", flowRate);
    snprintf(line1, sizeof(line1), "T:%5.2f L", totalLiters);
  } else {
    if (isnan(current)) snprintf(line0, sizeof(line0), "I: --- A");
    else                snprintf(line0, sizeof(line0), "I:%5.2f A", current);

    if (isnan(energy))  snprintf(line1, sizeof(line1), "E: --- kWh");
    else                snprintf(line1, sizeof(line1), "E:%6.3f kWh", energy);
  }

  utilityLcdLine(0, line0);
  utilityLcdLine(1, line1);
}

#endif
