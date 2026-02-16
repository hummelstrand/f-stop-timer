# f-stop-timer
A darkroom timer allowing exposure changes in EVs / f-stops. The timer controls a mains relay and uses readily available hardware; a TM1638 module with 8 buttons + 8 LEDs and an ESP12-1R-MV (ESP8266) board with a built-in 230 V relay.

This project is heavily indebted to Gavin Lyons https://github.com/glyons/Darkroom-Timer for the inspiration, concept, and hardware. As I added more features to his project I felt that the button-presses were often missed so I decided to restart the codebase with a fresh approach, while adding my own features.

## Hardware
- Controller: ESP12-1R-MV (ESP8266) with onboard 230 V AC mains relay
- Display and button input: TM1638 module (8-segments + 8 buttons + 8 LEDs)
- Rotary encoder input: COM-10982 (quadrature A/B + push switch)
- Buzzer: passive piezo

## Parts list (vendor-agnostic)
- ESP8266 relay module (ESP12-1R-MV or equivalent)
- TM1638 LED/Key module
- Rotary encoder with push button (quadrature A/B + SW, e.g., COM-10982)
- Passive piezo buzzer (3.3 V compatible)
- Jumper wires (Dupont) and hookup wire
- Enclosure, mains-rated cable/strain relief, and insulated terminals (for relay wiring)

## Implemented functionality:
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
- **Brightness:** Variable brightness set in code.
- **Rotary Encoder:** Adjust exposure time using a hardware encoder with acceleration for fast turns.

## Wiring notes
- Rotary encoder A/B/SW are wired to GPIO4, GPIO0, GPIO2 with internal pull-ups (active-low).
- GPIO0 and GPIO2 are boot-strap pins on the ESP8266, so keep them HIGH at boot. Do not hold the encoder button during reset/power-up.

## Pin table
| Function | Pin | Notes |
| --- | --- | --- |
| TM1638 STB | GPIO14 | STROBE_TM |
| TM1638 CLK | GPIO13 | CLOCK_TM |
| TM1638 DIO | GPIO12 | DIO_TM |
| Relay control | GPIO5 | RELAY_PIN |
| Buzzer | GPIO16 | BUZZER_PIN |
| Encoder A | GPIO4 | ENC_A_PIN (active-low, pull-up) |
| Encoder B | GPIO0 | ENC_B_PIN (active-low, pull-up, boot-strap) |
| Encoder SW | GPIO2 | ENC_SW_PIN (active-low, pull-up, boot-strap) |

## Wiring diagram (ASCII)
```
ESP12-1R-MV (ESP8266)           TM1638 Module
--------------------           -------------
GPIO14 (STB) -----------------> STB
GPIO13 (CLK) -----------------> CLK
GPIO12 (DIO) <----------------> DIO
GND        -------------------> GND
3V3        -------------------> VCC

ESP12-1R-MV (ESP8266)           Rotary Encoder (COM-10982)
--------------------           -------------------------
GPIO4  (ENC_A) ----------------> A/CLK
GPIO0  (ENC_B) ----------------> B/DT
GPIO2  (ENC_SW) ---------------> SW
GND        -------------------> GND

ESP12-1R-MV (ESP8266)
--------------------
GPIO5  (RELAY) ----> Onboard relay control (already connected on the baord)
GPIO16 (BUZZER) --> Buzzer + (Buzzer - to GND)
```

## Mains wiring (ASCII)
Safety note: Mains voltage is hazardous. Only wire the relay if you are qualified and follow local electrical codes. Use an insulated enclosure, strain relief, and proper terminals. Always disconnect power before working on wiring.

```
AC Live (L) -----> Relay COM
Relay NO --------> Enlarger Live (L)
AC Neutral (N) --> Enlarger Neutral (N)
AC Earth (PE) ---> Enlarger Earth (PE)

Notes:
- Use NO (normally open) for default-off behavior.
- NC (normally closed) is not used.
```

## Yet to be implemented:
- Strip-test mode based on "base exposure".
- If no "base exposure" has been set, clicking the "Strip Test" button should set the current exposure.

## Possible features:
- Mapping the encoder push button to a function.

## Features that probably won't be implemented:
- Brightness control of display and LEDs.
- Storing any values or settings to EEPROM (to ensure longevity of the hardware).
