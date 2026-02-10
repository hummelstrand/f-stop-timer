# f-stop-timer
A darkroom timer allowing exposure changes in EVs / f-stops. The timer controls a mains relay and uses standard hardware. 

This project is heavily indebted to https://github.com/glyons/Darkroom-Timer for the concept and hardware, but I felt that its button-presses were often missed so I decided to restart the codebase with a fresh approach, while adding my own features.

## Implemented functionality:
- Focus light relay control with timer counting up, and automatic shut off of relay after 120 s (to save the enlarger lamp from accidentally being left on).
- Exposure timer with +/- buttons with three speeds when held down continuously, plus relay control.
- Cancel button cancels both timers.
- Short beep every second for the FocusLight timer, and longer beep every ten seconds for both the FocusLight and the Exposure timer. (The beeper can only be turned off via a physical switch.)
- Configurable short/long beep durations and separate frequencies for the two beep types.
- Version display on startup, preceded by an all-segments/all-LEDs test.
- Brightness of display and LEDs set as variable in code.

## Yet to be implemented:
- Adjusting f-stop steps, and displaying currently selected step in display and LED.
- Rewiring the +/- buttons to increase/decrease the exposure timer based on chosen f-stop steps.
- If no base exposure set, then retain the current exposure timer increase/decrease in seconds rather than in f-stop steps.
- Setting/unsetting "base exposure" with button, and displaying it on the left display.
- Strip-test mode based on "base exposure".
- If no "base exposure" has been set, clicking the "Strip Test" button should set the current


## Possible features:
- Adding a rotary encoder would be a great feature.

## Features that won't be implemented:
- Brightness control via buttons.
- Storing any values or settings to EEPROM (to ensure longevity of the hardware).
