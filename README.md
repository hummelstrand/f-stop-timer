# f-stop-timer

A darkroom timer allowing exposure changes in EVs / f-stops. The timer controls a mains relay and uses readily available hardware; a TM1638 module with 8 buttons + 8 LEDs and an ESP12-1R-MV (ESP8266) board with a built-in 230 V relay.

This project is heavily indebted to Gavin Lyons <https://github.com/glyons/Darkroom-Timer> for the inspiration, concept, and hardware. As I added more features to his project I felt that the button-presses were often missed so I decided to restart the codebase with a fresh approach, while adding my own features.

## Hardware

- Controller: ESP12-1R-MV (ESP8266) with onboard 230 V AC mains relay
- Display and button input: TM1638 module (8-segments + 8 buttons + 8 LEDs)
- Rotary encoder input: COM-10982 (quadrature A/B; push switch not used)
- Buzzer: passive piezo

## Parts list (vendor-agnostic)

- ESP8266 relay module (ESP12-1R-MV or equivalent)
  reference: <https://devices.esphome.io/devices/esp-12f-relay-x1/>
- TM1638 LED/Key module
- Rotary encoder (quadrature A/B, e.g., COM-10982; SW not connected)
- Passive piezo buzzer (3.3 V compatible)
- Jumper wires (Dupont) and hookup wire
- Enclosure, mains-rated cable/strain relief, and insulated terminals (for relay wiring)

## Implemented functionality

- **Exposure Timer:** Count-down timer with relay control, adjustable in two modes:
  - **Seconds Mode:** Increase/decrease time in 0.1s increments.
  - **F-Stop Mode:** Set a "Base Exposure" and increase/decrease time in f-stop steps (e.g., +1 stop, -0.5 stop, etcetera).
- **F-Stop Logic:**
  - **Base Exposure (btn3):** Sets the current time as the base. Display shows difference in stops (e.g., "-1.00", " 0.33") and the calculated time.
  - **Step Selection (btn4):** Cycle between step sizes: 1.0 (1/1), 0.5 (1/2), 0.33 (1/3), 0.25 (1/4), and 0.17 (1/6) stops.
  - **LED Indication:** LEDs 3-7 indicate the selected step size when in F-Stop Mode.
  - **Step Preview:** Briefly shows "STEP X.XX" when changing the step size.
- **Focus Light Control:** Relay control with count-up timer (great for burning in with additional exposure without having to set a timer). Automatic shut-off after 120s. Toggle on/off or hold down the button to run and release to stop.
- **Cancel Button:** Stops all timers and returns to normal state.
- **Audio Feedback:** Long beep every 10 seconds for both timers.
- **Startup:** Version display preceded by an all-segments/all-LEDs test.
- **Input/Output diagnostic mode:** Hold `btn8` during boot to enter a persistent diagnostic mode. All LEDs stay ON, relay is forced OFF, all segments are shown for 1 second, then display shows button and encoder activity (`B0`, `B1`..`B8`, `BM`; and `E-`, `E0`, `E+`).
- **Brightness:** Variable brightness set in code.
- **Rotary Encoder:** Adjust exposure time with acceleration for fast turns (replaces btn6/btn7).

## Wiring notes

- Encoder 1: A/B wired to GPIO4, GPIO3 `RX` with internal pull-ups (active-low). **Note: GPIO3 is physically labeled as `RX` on the board's header.**
- Encoder push-switch pins (SW) are intentionally left unconnected.
- GPIO0 (IO0) is intentionally unused.

## ESP8266 boot-pin and unused-pin notes

The ESP8266 samples certain pins during reset to decide boot mode. These pins must be at the correct level at boot:

| Pin | Required level at boot | If wrong at boot |
| --- | --- | --- |
| **GPIO0** | **HIGH** | Enters programming/flash mode instead of running sketch |
| **GPIO2** | **HIGH** | Boot can fail or enter invalid boot mode |
| **GPIO15** | **LOW** | Boot can fail or enter invalid boot mode |

Why this can cause problems:

- Buttons and rotary encoder are mechanical contacts and can bounce or momentarily short to GND/VCC during reset.
- If they are wired directly to boot-strap pins, they can force the wrong level exactly when the ESP8266 samples those pins.
- Result: the board may not boot your timer sketch reliably (or may appear "dead" until reset/reflash).

Why GPIO0 is intentionally unused:

- **GPIO0 (IO0)** is left unused as a user input because any accidental LOW during reset puts the chip into flashing mode.
- This prevents hard-to-diagnose startup failures caused by external switches/encoder connected to GPIO0.

Other intentionally avoided or special-purpose pins:

- **GPIO2/GPIO15** are avoided by default in this project for the same boot-strap reason.
- **GPIO6–GPIO11** are connected to onboard flash memory on ESP-12 class modules and should not be used for external I/O.
- **GPIO1/GPIO3** are UART TX/RX; they are usable but can interfere with serial logging/flashing if loaded incorrectly.

## Boot-safe wiring checklist (ESP8266)

Before powering up:

- Leave **GPIO0** unconnected for user controls (do not wire buttons/encoder contacts to it).
- Keep **GPIO2/GPIO15** free from mechanical inputs unless you intentionally design around boot constraints.
- Verify no external circuit can pull **GPIO0** or **GPIO2** LOW during reset.
- Verify no external circuit can pull **GPIO15** HIGH during reset.
- Avoid connecting noisy/mechanical signals directly to boot-strap pins.
- If startup is unreliable, disconnect external inputs and confirm the board boots consistently first, then reconnect signals one by one.

## Input/Output diagnostic mode

Use this mode for quick hardware verification of TM1638 inputs and encoder direction.

- Enter mode: hold **btn8** during power-up/reset.
- Relay behavior: relay is forced OFF while mode is active.
- LED behavior: all 8 TM1638 LEDs stay ON continuously.
- Display behavior:
  - First 1 second: all segments ON (`8.8.8.8.8.8.8.8.`).
  - Then live diagnostics:
    - Buttons: `B0` (none), `B1`..`B8` (single pressed), `BM` (multiple buttons pressed).
    - Encoder 1 direction: `E-` (counter-clockwise), `E+` (clockwise), `E0` (idle).
- Exit mode: power-cycle or reset without holding **btn8**.

## Pin table

| Function | Pin | Notes |
| --- | --- | --- |
| TM1638 STB | GPIO14 | STROBE_TM |
| TM1638 CLK | GPIO13 | CLOCK_TM |
| TM1638 DIO | GPIO12 | DIO_TM |
| Relay control | GPIO5 | RELAY_PIN |
| Buzzer | GPIO16 | BUZZER_PIN |
| Encoder 1 A | GPIO4 | ENC1_A_PIN (active-low, pull-up) |
| Encoder 1 B | GPIO3 `RX` | ENC1_B_PIN (active-low, pull-up). **Labeled as `RX` on the board.** |

## Wiring diagram (ASCII)

```text
ESP12-1R-MV (ESP8266)           TM1638 Module
--------------------           -------------
GPIO14 (STB) -----------------> STB
GPIO13 (CLK) -----------------> CLK
GPIO12 (DIO) <----------------> DIO
GND        -------------------> GND
3V3        -------------------> VCC

ESP12-1R-MV (ESP8266)           Rotary Encoder 1
--------------------           ----------------
GPIO4  (ENC1_A) --------------> A/CLK
GPIO3 / RX (ENC1_B) ----------> B/DT
GND        -------------------> C (common)
SW         -------------------> (not connected)

ESP12-1R-MV (ESP8266)
--------------------
GPIO5  (RELAY) ----> Onboard relay control (already connected on the baord)
GPIO16 (BUZZER) --> Buzzer + (Buzzer - to GND)
```

## Mains wiring (ASCII)

Safety note: Mains voltage is hazardous. Only wire the relay if you are qualified and follow local electrical codes. Use an insulated enclosure, strain relief, and proper terminals. Always disconnect power before working on wiring.

```text
AC Live (L) -----> Relay COM
Relay NO --------> Enlarger Live (L)
AC Neutral (N) --> Enlarger Neutral (N)
AC Earth (PE) ---> Enlarger Earth (PE)

Notes:
- Use NO (normally open) for default-off behavior.
- NC (normally closed) is not used.
```

## Yet to be implemented

- Strip-test mode based on "base exposure".
- If no "base exposure" has been set, clicking the "Strip Test" button should set the current exposure.

## Possible features

- Add switch or LED to the encoder (requires freeing up GPIO pins).

## Features that probably won't be implemented

- Brightness control of display and LEDs.
- Storing any values or settings to EEPROM (to ensure longevity of the hardware).
