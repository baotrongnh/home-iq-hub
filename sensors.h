#ifndef SENSORS_H
#define SENSORS_H

/*
 * sensors.h — Flow Sensor & PZEM004T Power Meter
 * Doc du lieu cam bien do nuoc va dien.
 */

// ===== Flow Sensor State =====
volatile int pulseCount = 0;
float flowRate    = 0.0f;
float totalLiters = 0.0f;

// ===== PZEM State =====
float current = NAN;
float energy  = NAN;

// ===== Timing =====
unsigned long lastFlowMs     = 0;
unsigned long lastPzemReadMs = 0;

// ===== ISR: Flow sensor pulse counter =====
void IRAM_ATTR pulseCounter() {
  pulseCount++;
}

// ===== Flow sensor: tinh luu luong moi giay =====
void handleFlowSensor() {
  unsigned long now = millis();
  if (now - lastFlowMs < FLOW_INTERVAL_MS) return;
  lastFlowMs = now;

  noInterrupts();
  int count = pulseCount;
  pulseCount = 0;
  interrupts();

  flowRate = count / PULSE_FREQUENCY;
  totalLiters += flowRate / PULSE_FREQUENCY;
}

// ===== PZEM: doc dong dien & nang luong =====
void handlePzemPoll() {
  unsigned long now = millis();
  if (now - lastPzemReadMs < PZEM_READ_INTERVAL_MS) return;
  lastPzemReadMs = now;

  current = pzem.current();
  energy  = pzem.energy();

  if (isnan(current) || isnan(energy)) {
    Serial.println("[PZEM] Read error (NaN)");
  }
}

#endif
