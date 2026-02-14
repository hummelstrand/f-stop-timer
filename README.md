# f-stop-timer
A darkroom timer allowing exposure changes in EVs / f-stops. The timer controls a mains relay and uses readily available hardware; a TM1638 module with 8 buttons + 8 LEDs and an ESP12-1R-MV (ESP32) board with a built-in 230 V relay. 

This project is heavily indebted to Gavin Lyons https://github.com/glyons/Darkroom-Timer for the inspiration, concept, and hardware. As I added more features to his project I felt that the button-presses were often missed so I decided to restart the codebase with a fresh approach, while adding my own features.

## Implemented functionality:
- **Exposure Timer:** Count-down timer with relay control, adjustable in two modes:
    - **Seconds Mode:** Increase/decrease time in 0.1s increments.
    - **F-Stop Mode:** Set a "Base Exposure" and increase/decrease time in f-stop steps (e.g., +1 stop, -0.5 stop, etcetera).
- **F-Stop Logic:**
    - **Base Exposure (btn3):** Sets the current time as the base. Display shows difference in stops (e.g., "-1.00", " 0.33") and the calculated time.
    - **Step Selection (btn4):** Cycle between step sizes: 1.0 (1/1), 0.5 (1/2), 0.33 (1/3), 0.25 (1/4), and 0.17 (1/6) stops. 
    - **LED Indication:** LEDs 3-7 indicate the selected step size when in F-Stop Mode.
- **Focus Light Control:** Relay control with count-up timer (great for burning in with additional exposure without having to set a timer). Automatic shut-off after 120s. Toggle on/off or hold down the button to run and release to stop.
- **Cancel Button:** Stops all timers and returns to normal state.
- **Audio Feedback:** Long beep every 10 seconds for both timers.
- **Startup:** Version display preceded by an all-segments/all-LEDs test.
- **Brightness:** Variable brightness set in code.

## Yet to be implemented:
- Strip-test mode based on "base exposure".
- If no "base exposure" has been set, clicking the "Strip Test" button should set the current exposure.

## Possible features:
- Adding a rotary encoder would be a great feature.

## Features that won't be implemented:
- Brightness control via buttons.
- Storing any values or settings to EEPROM (to ensure longevity of the hardware).
