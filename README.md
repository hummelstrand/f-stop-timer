# f-stop-timer
A darkroom timer allowing exposure changes in EVs / f-stops. The timer controls a mains relay and uses standard hardware. 

This project is heavily indebted to https://github.com/glyons/Darkroom-Timer for the concept and hardware, but I felt that its button-presses were often missed so I decided to restart the codebase with a fresh approach, while adding my own features.

## Implemented functionality:
- Focus light relay control with timer counting up, and automatic shut off of relay after 120 s.
- Exposure timer with +/- buttons with two speeds when held down continuously, plus relay control.
- Cancel button cancels both timers.

## Yet to be implemented:
- Adjusting f-stop steps, and displaying currently selected step in display and LED.
- Rewiring the +/- buttons to increase/decrease the exposure timer based on chosen f-stop steps.
- If no base exposure set, then retain the current exposure timer increase/decrease in seconds rather than in f-stop steps.
- Setting/unsetting "base exposure" with button, and displaying it on the left display.
- Strip-test mode based on "base exposure".
- If no "base exposure" has been set, clicking the "Strip Test" button should set the current
- Beeper every second (turned off via physical switch).
- Brightness of display and LEDs (set as variable in code).
- Display version number at power on.

## Possible features:
- Adding a rotary encoder would be a great feature.

## Features that won't be implemented:
- Brightness control via buttons.
- Storing any values or settings to EEPROM (to ensure longevity of the hardware).
