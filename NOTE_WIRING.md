# SmartApartmentEsp32 - Wiring Note

> Updated: 2026-03-29 — Dong bo voi code hien tai

## 1) Pin Mapping

| Feature                  | Signal             | ESP32 Pin | Mode                              |
| ------------------------ | ------------------ | --------: | --------------------------------- |
| Light 1                  | LED output         |    GPIO18 | OUTPUT                            |
| Light 2                  | LED output         |    GPIO19 | OUTPUT                            |
| Flame sensor             | Flame input        |    GPIO27 | INPUT                             |
| SOS LED                  | Alarm LED          |    GPIO26 | OUTPUT                            |
| Buzzer                   | Alarm buzzer       |    GPIO25 | OUTPUT                            |
| Flow sensor              | Pulse input        |    GPIO32 | INPUT_PULLUP + interrupt          |
| Stop alarm button        | Button input       |    GPIO33 | INPUT                             |
| Screen toggle button     | Button input       |    GPIO14 | INPUT_PULLUP                      |
| Curtain servo            | Servo signal       |    GPIO23 | OUTPUT (PWM 50Hz)                 |
| Lock relay (K01 control) | Relay control      |    GPIO13 | OUTPUT                            |
| Lock feedback OPEN       | Feedback input     |    GPIO34 | INPUT (external pull-up required) |
| Lock feedback CLOSE      | Feedback input     |    GPIO35 | INPUT (external pull-up required) |
| Curtain limit OPEN       | Limit switch input |     GPIO5 | INPUT_PULLUP                      |
| Curtain limit CLOSE      | Limit switch input |     GPIO4 | INPUT_PULLUP                      |
| PZEM004T RX              | UART RX            |    GPIO16 | UART2 RX                          |
| PZEM004T TX              | UART TX            |    GPIO17 | UART2 TX                          |

## 2) I2C Devices

- LCD main (keypad/password) at address `0x26`
- LCD utility (water/electric) at address `0x27`
- Keypad I2C expander at address `0x20`

Default I2C pins (unchanged):

- SDA: GPIO21
- SCL: GPIO22

## 3) PZEM004T Wiring

- PZEM TX -> ESP32 RX (GPIO16)
- PZEM RX -> ESP32 TX (GPIO17)
- GND PZEM <-> GND ESP32 (common ground)
- Power the PZEM module per manufacturer spec.

Important:

- Never connect ESP32 directly to high-voltage lines.
- Follow PZEM004T isolation and CT installation instructions.

## 4) K01-12V Lock + Feedback Wiring

### Control (unlock pulse)

- ESP32 GPIO13 -> relay module IN
- Relay module controls 12V line to K01 lock
- Relay and ESP32 must share GND reference (or use proper opto-isolated module)

### Feedback

- K01 OPEN feedback -> GPIO34
- K01 CLOSE feedback -> GPIO35
- Use external pull-up resistors (10k to 3.3V) for GPIO34/35
- Feedback contact should pull pin to GND when active (active-low logic)
- Code logs state changes to Serial (see `handleDoorFeedback()` in `door.h`)

## 5) Curtain Limit Switch Wiring

- OPEN end limit switch -> GPIO5
- CLOSE end limit switch -> GPIO4
- Internal pull-up enabled in code (`INPUT_PULLUP`)
- Switch active state pulls pin to GND (active-low)

Behavior:

- If moving OPEN and OPEN limit is hit, motor stops immediately.
- If moving CLOSE and CLOSE limit is hit, motor stops immediately.

## 6) Button Wiring

- GPIO33 (Stop alarm): Use button between pin and GND with external pull-up.
- GPIO14 (Screen toggle): Use button between pin and GND. Internal pull-up enabled.

## 7) Power and Ground Recommendations

- Servo, lock, buzzer should use adequate external supply.
- Keep ESP32 3.3V logic isolated from 12V lock circuit via relay/opto.
- Always connect grounds correctly to avoid floating inputs and unstable readings.

## 8) Boot Pin Note

GPIO4 and GPIO5 are strapping-related on some ESP32 boards.

- Do not hold limit switches active while powering up/reset.
- If boot instability appears, move limit switches to safer GPIO and update `config.h`.
