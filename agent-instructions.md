# Agent instructions for F-Stop Timer

## Quick context
This is an Arduino-based darkroom enlarger timer project. The primary active sketch is `f-stop-timer.ino` which implements two timer modes: FocusLight Timer (count-up) and Exposure Timer (countdown). Legacy sketches live in the `legacy_darkroom_timer/` folder. A local Arduino library `TM1638plus` under `libraries/TM1638plus/` provides the display (8-segment LED) and button interface.

## Big picture
- Hardware: TM1638 module with 8 buttons, 8 LEDs, plus relay and buzzer
- The code is organized into sections: Hardware Interface (display, LEDs, relay, button reading), Button Handling (switch statement for each button), and Timer Updates (per-loop execution)
- Platform support: Conditional compilation handles Arduino (`__AVR__`) and ESP8266 (`ESP8266`) targets; `HIGH_FREQ` flag optimizes TM1638 timing for high-clock MCUs
- Pin mappings at the top of `f-stop-timer.ino`: STROBE_TM, CLOCK_TM, DIO_TM (display), RELAY_PIN (5), BUZZER_PIN (16)

## Timer functionality

### FocusLight Timer (btn2)
- Count-up timer: short press toggles on/off; long press (hold > 300ms) runs timer while held
- Maximum: 120 seconds
- LED2 (position 1) lights while running, relay turns ON
- Display format: "FOC  " + time (e.g., "FOC  12.6")
- Continuous press: timer runs and stops on button release
- Short press: toggles between running and cleared states
- Buzzer: short beep every second and long beep every 10 seconds (long overrides short on 10s)

### Exposure Timer (btn6/btn7/btn8)
- Count-down timer; adjustable via btn6 (decrease) and btn7 (increase) in 0.1s steps
- Range: 0.1s to 999.9s; default: 8.0s
- Auto-repeat on btn6/btn7: initial repeat every 200ms, speeds up to 50ms after 2s continuous hold
- btn8: Start → Pause → Resume cycle
  - Start: begins countdown, relay ON, LED8 (position 7) ON
  - Pause: halts countdown, relay OFF, LED8 OFF, saves remaining time
  - Resume: resumes from paused time, relay ON, LED8 ON
- When countdown reaches 0, relay turns OFF, LED8 OFF, display shows normal state
- Buzzer: long beep every 10 seconds

### Normal State
- Displays current `exposureTimerValue` (e.g., "    8.0")
- All LEDs off, relay OFF
- Shown when no timers are running

### Cancel (btn1)
- Single-press only (ignores continuous hold)
- Stops any running/paused timer, displays " CANCEL " for 500ms, returns to normal state
- Preserves `exposureTimerValue` for next timer session

## Timing constants (all in milliseconds)
- `AUTO_REPEAT_INTERVAL_ULTRA`: 5 ms (ultra fast repeat rate after extended hold)
- `AUTO_REPEAT_FAST_THRESHOLD`: 2000 ms (when to switch to fast repeat)
- `AUTO_REPEAT_ULTRA_THRESHOLD`: 3000 ms (when to switch to ultra fast repeat)
- `STARTUP_ALL_ON_MS`: duration to show all segments/LEDs on startup

## App version
- `BUZZER_SHORT_FREQUENCY`: short beep frequency (Hz)
- `BUZZER_LONG_FREQUENCY`: long beep frequency (Hz)

## Button mappings
- btn1 (bit 1): Cancel all timers
- btn2 (bit 2): FocusLight Timer control
- btn3–btn5 (bits 4, 8, 16): Currently unused
- btn6 (bit 32): Decrease Exposure Timer value
- btn7 (bit 64): Increase Exposure Timer value
- btn8 (bit 128): Start/Pause/Resume Exposure Timer countdown

## Code structure & patterns
- **Hardware Interface**: `hwSetDisplay()`, `hwSetLED()`, `hwSetLEDs()`, `hwSetRelay()`, `hwReadButton()`, `hwCheckContinuousPress()` provide abstraction layer
- **Button press tracking**: Each button has a `btnXPressHandled` flag; reset on button release
- **Continuous press detection**: `hwCheckContinuousPress()` manages `continuousPressDetected`, `buttonPressStartTime`, `lastAutoRepeatTime`
- **Timer functions**: `focusTimerStart()`, `focusTimerStop()`, `focusTimerClear()`, `focusTimerUpdate()` handle FocusLight logic
- **Normal state**: `setNormalState()` centralizes display/relay/LED reset
- **Loop structure**: Button handling in switch/case, then timer updates, then state tracking

## Integration points & dependencies
- Local library: `libraries/TM1638plus/` — API used: `displayText()`, `readButtons()`, `setLED()`, `setLEDs()`, `displayBegin()`
- Serial output at 9600 baud for debugging messages
- No external CI/tests

## Safe change guidelines
- Pin mappings: defined at top of file, conditional on platform (`__AVR__` vs `ESP8266`)
- Timing constants: all in one place at the top; adjust `CONTINUOUS_PRESS_THRESHOLD`, `AUTO_REPEAT_INTERVAL`, etc. here
- Relay/LED control: through hardware interface functions to avoid coupling
- Display format: strings use `sprintf()` with "FOC  %4.1f" for FocusLight and "    %4.1f" for Exposure
- Boolean state tracking: careful attention to `focusTimerRunning`, `exposureTimerRunning`, `exposureTimerPaused` to avoid race conditions in loop

## Files to know
- Main sketch: `f-stop-timer.ino`
- Library header: `libraries/TM1638plus/src/TM1638plus.h`
- Project info: `README.md`
- Legacy code (for reference): `legacy_darkroom_timer/`

## How to build & run
- Arduino IDE: Open `f-stop-timer.ino`, select board (e.g., Arduino Uno for `__AVR__`), compile & upload
- Arduino CLI example:
  ```
  arduino-cli compile --fqbn arduino:avr:uno /path/to/f-stop-timer.ino
  arduino-cli upload -p /dev/ttyUSB0 --fqbn arduino:avr:uno /path/to/f-stop-timer.ino
  ```
