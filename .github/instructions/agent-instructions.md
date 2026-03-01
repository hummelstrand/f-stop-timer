# Agent instructions for F-Stop Timer

## Quick context

This is an Arduino-based darkroom enlarger timer project. The primary active sketch is `f-stop-timer.ino` which implements two timer modes: FocusLight Timer (count-up) and Exposure Timer (countdown), plus a rotary encoder for exposure adjustments. A local Arduino library `TM1638plus` under `libraries/TM1638plus/` provides the display (8-segment LED) and button interface.

## Big picture

- Hardware: TM1638 module with 8 buttons, 8 LEDs and an 8-segment display. An ESP12-1R-MV (ESP8266) with 230 V relay. An external buzzer. A rotary encoder (A/B only, no switches/LEDs).
- The code is organized into sections: Hardware Interface (display, LEDs, relay, button reading), Button Handling (switch statement for each button), and Timer Updates (per-loop execution)
- Platform support: Conditional compilation handles Arduino (`__AVR__`) and ESP8266 (`ESP8266`) targets; `HIGH_FREQ` flag optimizes TM1638 timing for high-clock MCUs
- Pin mappings at the top of `f-stop-timer.ino`: STROBE_TM, CLOCK_TM, DIO_TM (display), RELAY_PIN (5), BUZZER_PIN (16), ENC1_A_PIN (4), ENC1_B_PIN (3); GPIO0/IO0 intentionally unused
- Display/LED brightness is configurable via `DISPLAY_LED_BRIGHTNESS` and applied by `hwApplyBrightness()` (range 0–7)

## Timer functionality

### FocusLight Timer (btn2)

- Count-up timer: short press toggles on/off; long press (hold > 300ms) runs timer while held
- Maximum: 120 seconds
- LED1 (position 0) lights while running, relay turns ON
- Display format: "FOC  " + time (e.g., "FOC  12.6")
- Continuous press: timer runs and stops on button release
- Short press: toggles between running and cleared states
- Buzzer: long beep every 10 seconds

### Exposure Timer (btn6/btn7/btn8)

- Count-down timer; adjustable via btn6 (decrease) and btn7 (increase)
- **Seconds Mode**: default behavior; decreases/increases in 0.1s steps. Values are rounded to 1 decimal place to prevent floating-point drift.
- **F-Stop Mode**: active when Base Exposure is set (via btn3); decreases/increases by selected f-stop steps (via btn4)
- Range: 0.1s to 999.9s; default: 8.0s
- Auto-repeat on btn6/btn7: same acceleration logic as FocusLight
- btn8: Start → Pause → Resume cycle
  - Start: begins countdown, relay ON, LED8 (position 7) ON
  - Pause: halts countdown, relay OFF, LED8 OFF, saves remaining time
  - Resume: resumes from paused time, relay ON, LED8 ON
- When countdown reaches 0, relay turns OFF, LED8 OFF, display shows normal state
- Buzzer: long beep every 10 seconds

### Base Exposure / F-Stop Mode (btn3/btn4)

- **btn3**: Toggles Base Exposure mode.
  - Sets current exposure time as the "Base" value.
  - Display splits: Left side shows "BASE" (if value == base) or f-stop difference (e.g., "-1.00", " 0.33"); Right side shows time.
  - Spacing between left/right parts reduces to 0 if needed (e.g. "BASE100.5") to preserve decimal precision for values > 99.9.
  - LEDs 3-7 indicate the selected f-stop step size.
- **btn4**: Cycles f-stop step sizes (1.0, 0.5, 0.333333, 0.25, 0.166667).
  - Briefly displays "STEP X.XX" (e.g., "STEP 0.33") for 1 second.
  - Step size selection is persistent.

### Rotary Encoder

- **Encoder 1 (GPIO4/GPIO3):** Alternate input for exposure adjustments when no timers are running (replaces btn6/btn7). Acceleration: faster detents apply larger steps (multipliers 4x and 10x). **Note: GPIO3 is physically labeled as `RX` on the board.**
- Encoder push-switch pins (SW) are not used and should remain unconnected.

### Input/Output diagnostic mode

- Enter by holding **btn8** at boot/reset.
- Purpose: verify TM1638 button reads and Encoder 1 direction without entering normal timer logic.
- Relay is forced OFF in this mode.
- All TM1638 LEDs stay ON continuously.
- Display behavior:
  - First 1 second: all segments ON (`8.8.8.8.8.8.8.8.`)
  - Then live diagnostics: buttons (`B0`, `B1`..`B8`, `BM`) and encoder direction (`E-`, `E0`, `E+`).
- Mode is latched for that boot; exit by rebooting without holding **btn8**.

## ESP8266 boot-pin safety notes

The ESP8266 samples boot-strap pins during reset. Required levels at boot:

| Pin | Required boot level | Failure mode if incorrect |
| --- | --- | --- |
| **GPIO0** | **HIGH** | Enters programming/flash mode instead of normal sketch boot |
| **GPIO2** | **HIGH** | Boot failure or invalid boot mode |
| **GPIO15** | **LOW** | Boot failure or invalid boot mode |

Why these pins can cause problems:

- Mechanical contacts (buttons/encoder) can bounce and briefly force LOW/HIGH during reset.
- If that glitch occurs at boot sampling time, the ESP8266 may not start the application.

Why GPIO0 is intentionally unused:

- **GPIO0/IO0** is intentionally unused as a user input because accidental LOW at reset puts the board into flashing mode.
- Avoiding GPIO0 prevents intermittent "won't boot" behavior caused by external switches/encoder.

## Why some GPIOs are intentionally unused

- **GPIO0/IO0** is intentionally unused as input to avoid accidental LOW at boot (programming mode).
- **GPIO2/GPIO15** are avoided by default for the same boot-strap reason.
- **GPIO6–GPIO11** are reserved for onboard flash on ESP-12 modules and should not be used as external I/O.
- **GPIO1/GPIO3** are UART TX/RX and can interfere with flashing/serial debugging if heavily loaded.

## Boot-safe wiring checklist (ESP8266)

- Keep **GPIO0** free from user inputs (no button/encoder wiring).
- Keep **GPIO2/GPIO15** free from mechanical inputs unless explicitly designing for boot-strap requirements.
- Ensure external wiring cannot pull **GPIO0/GPIO2** LOW during reset.
- Ensure external wiring cannot pull **GPIO15** HIGH during reset.
- If boot issues appear, test with external inputs disconnected, then reconnect one signal at a time.

### Normal State

- Displays current `exposureTimerValue` (right-aligned, 1 decimal place, e.g., "   12.5")
- If Base Exposure is set: shows split display (Diff + Time) and LEDs 3-7 active
- Relay OFF
- Shown when no timers are running

### Cancel (btn1)

- Single-press only (ignores continuous hold)
- Stops any running/paused timer, displays " CANCEL " for 500ms, returns to normal state
- Preserves `exposureTimerValue` and Base Exposure settings

## Timing constants (all in milliseconds)

- `CONTINUOUS_PRESS_THRESHOLD`: 300
- `AUTO_REPEAT_INTERVAL`: 200
- `AUTO_REPEAT_INTERVAL_FAST`: 50
- `AUTO_REPEAT_INTERVAL_ULTRA`: 2
- `AUTO_REPEAT_FAST_THRESHOLD`: 1500
- `AUTO_REPEAT_ULTRA_THRESHOLD`: 3000
- `STARTUP_ALL_ON_MS`: 400
- `VERSION_DISPLAY_MS`: 1100
- `BOOT_DIAG_SEGMENT_HOLD_MS`: 1000
- `BOOT_DIAG_ENCODER_INDICATOR_MS`: 250
- `STEP_DISPLAY_DURATION`: 1000
- `ENCODER_DEBOUNCE_MS`: 2
- `ENCODER_ACCEL_FAST_MS`: 60
- `ENCODER_ACCEL_ULTRA_MS`: 30

## App version

- `APP_VERSION`: "VERS 0.9.1"
- `BUZZER_SHORT_FREQUENCY`: 2400 Hz
- `BUZZER_LONG_FREQUENCY`: 440 Hz
- `BUZZER_SHORT_BEEP_MS`: 40
- `BUZZER_LONG_BEEP_MS`: 160

## Button mappings

- btn1 (bit 1): Cancel all timers
- btn2 (bit 2): FocusLight Timer control
- btn3 (bit 4): Set/Unset Base Exposure (toggle F-Stop Mode)
- btn4 (bit 8): Cycle f-stop step size
- btn5 (bit 16): Currently unused
- btn6 (bit 32): Decrease Exposure Timer value (seconds or f-stops)
- btn7 (bit 64): Increase Exposure Timer value (seconds or f-stops)
- btn8 (bit 128): Start/Pause/Resume Exposure Timer countdown

## Code structure & patterns

- **Hardware Interface**: `hwSetDisplay()`, `hwSetLED()`, `hwSetLEDs()`, `hwSetRelay()`, `hwReadButton()`, `hwCheckContinuousPress()` provide abstraction layer
- **Button press tracking**: Each button has a `btnXPressHandled` flag; reset on button release
- **Continuous press detection**: `hwCheckContinuousPress()` manages `continuousPressDetected`, `buttonPressStartTime`, `lastAutoRepeatTime`
- **Focus Timer functions**: `focusTimerStart()`, `focusTimerStop()`, `focusTimerClear()`, `focusTimerUpdate()` handle FocusLight logic
- **Exposure Timer functions**: `startExposureTimer()`, `pauseExposureTimer()`, `resumeExposureTimer()`, `stopExposureTimer()`, `updateExposureTimer()`, `handleExposureChange()` encapsulate exposure logic
- **Encoder functions**: `readEncoder1Step()` / `handleEncoder1ExposureStep()` for exposure
- **Normal state**: `setNormalState()` centralizes display/relay/LED reset
- **Loop structure**: Button handling in switch/case delegates to helper functions; `updateExposureTimer()` called explicitly

## Integration points & dependencies

- Local library: `libraries/TM1638plus/` — API used: `displayText()`, `readButtons()`, `setLED()`, `setLEDs()`, `displayBegin()`
- Serial output at 9600 baud for debugging messages
- No external CI/tests

## Safe change guidelines

- Pin mappings: defined at top of file, conditional on platform (`__AVR__` vs `ESP8266`)
- Timing constants: all in one place at the top; adjust `CONTINUOUS_PRESS_THRESHOLD`, `AUTO_REPEAT_INTERVAL`, etc. here
- Relay/LED control: through hardware interface functions to avoid coupling
- Brightness: use `DISPLAY_LED_BRIGHTNESS` (0 = dimmest, 7 = brightest) and call `hwApplyBrightness()` after `tm.displayBegin()`
- Display format: strings use `sprintf()` with "FOC  %4.1f" for FocusLight and right-aligned decimal formatting for Exposure
- Boolean state tracking: careful attention to `focusTimerRunning`, `exposureTimerRunning`, `exposureTimerPaused` to avoid race conditions in loop

## Files to know

- Main sketch: `f-stop-timer.ino`
- Library header: `libraries/TM1638plus/src/TM1638plus.h`
- Project info: `README.md`
- Legacy code (for reference): `legacy_darkroom_timer/`

## How to build & run

- Arduino IDE: Open `f-stop-timer.ino`, select board (e.g., Arduino Uno for `__AVR__`), compile & upload
- Arduino CLI example:

  ```bash
  arduino-cli compile --fqbn arduino:avr:uno /path/to/f-stop-timer.ino
  arduino-cli upload -p /dev/ttyUSB0 --fqbn arduino:avr:uno /path/to/f-stop-timer.ino
  ```
