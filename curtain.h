#ifndef CURTAIN_H
#define CURTAIN_H

/*
 * curtain.h — Curtain Motor Control
 * Dieu khien rem cua bang servo + limit switch.
 */

// ===== Curtain State =====
bool curtainCmdOpen  = false;
bool curtainCmdClose = false;
bool curtainAtOpen   = false;
bool curtainAtClose  = false;

// ===== Xu ly motor rem cua =====
void handleCurtain() {
  bool openLimitActive  = (digitalRead(LIMIT_SWITCH_OPEN_PIN) == LOW);
  bool closeLimitActive = (digitalRead(LIMIT_SWITCH_CLOSE_PIN) == LOW);

  // Dung khi cham limit switch MO
  if (openLimitActive && !curtainAtOpen) {
    curtainAtOpen  = true;
    curtainAtClose = false;
    curtainCmdOpen = false;
    curtainServo.write(SERVO_STOP_PWM);
    return;
  }

  // Dung khi cham limit switch DONG
  if (closeLimitActive && !curtainAtClose) {
    curtainAtClose  = true;
    curtainAtOpen   = false;
    curtainCmdClose = false;
    curtainServo.write(SERVO_STOP_PWM);
    return;
  }

  // Dieu khien motor theo lenh
  if (curtainCmdOpen && !curtainAtOpen) {
    curtainServo.write(SERVO_OPEN_PWM);
  } else if (curtainCmdClose && !curtainAtClose) {
    curtainServo.write(SERVO_CLOSE_PWM);
  } else {
    curtainServo.write(SERVO_STOP_PWM);
  }
}

#endif
