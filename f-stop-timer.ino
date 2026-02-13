void setNormalState();

/*
  F-Stop Timer
  
  A darkroom enlarger timer with FocusLight and Exposure Timer modes.
  
  Hardware: TM1638plus module with 8 buttons, 8 LEDs, relay, buzzer
  Uses the existing TM1638plus library
*/

#include <TM1638plus.h>
#include <string.h>

// GPIO I/O pins on the Arduino
#if defined(__AVR__)
  // Arduino
  #define STROBE_TM 13
  #define CLOCK_TM 12
  #define DIO_TM 11
  #define HIGH_FREQ false  // false for Arduino Uno (~16 MHz)
#elif defined(ESP8266)
  // ESP8266 specific code here
  #define STROBE_TM 14
  #define CLOCK_TM 13
  #define DIO_TM 12
  #define HIGH_FREQ true   // true for high freq CPU > ~100 MHz
#endif

#define RELAY_PIN 5 // Relay control pin
#define BUZZER_PIN 16 // Buzzer control pin

// App version
const char APP_VERSION[] = "VERS 0.9.0";
const unsigned long VERSION_DISPLAY_MS = 1100;
const unsigned long STARTUP_ALL_ON_MS = 400;

// Display/LED brightness (0 = dimmest, 7 = brightest)
const uint8_t DISPLAY_LED_BRIGHTNESS = 0;

// Buzzer configuration
const unsigned int BUZZER_SHORT_FREQUENCY = 2400; // Hz
const unsigned int BUZZER_LONG_FREQUENCY = 440; // Hz
const unsigned int BUZZER_SHORT_BEEP_MS = 40; // FocusLight short beep duration
const unsigned int BUZZER_LONG_BEEP_MS = 160; // FocusLight/Exposure long beep duration

// Button definitions (bit patterns from TM1638)
#define btn1 1
#define btn2 2
#define btn3 4
#define btn4 8
#define btn5 16
#define btn6 32
#define btn7 64
#define btn8 128

// LED definitions (bit patterns)
#define led1 1
#define led2 2
#define led3 4
#define led4 8
#define led5 16
#define led6 32
#define led7 64
#define led8 128

// Create TM1638plus object
TM1638plus tm(STROBE_TM, CLOCK_TM, DIO_TM, HIGH_FREQ);

// Button and display state
uint8_t lastButton = 0;

// Continuous button press detection
const unsigned long CONTINUOUS_PRESS_THRESHOLD = 300; // milliseconds
const unsigned long AUTO_REPEAT_INTERVAL = 200; // milliseconds between auto-repeats (initial)
const unsigned long AUTO_REPEAT_INTERVAL_FAST = 50; // milliseconds between auto-repeats (after prolonged press)
const unsigned long AUTO_REPEAT_INTERVAL_ULTRA = 2; // milliseconds between auto-repeats (ultra fast)
const unsigned long AUTO_REPEAT_FAST_THRESHOLD = 1500; // milliseconds before switching to fast repeat
const unsigned long AUTO_REPEAT_ULTRA_THRESHOLD = 3000; // milliseconds before switching to ultra fast repeat
unsigned long buttonPressStartTime = 0; // When the button was first pressed
bool continuousPressDetected = false; // Whether continuous press has been identified
unsigned long lastAutoRepeatTime = 0; // Last time auto-repeat triggered
bool btn1PressHandled = false; // For single press on btn1 (cancel)

// FocusLight Timer functionality for btn2
bool focusTimerRunning = false; // Whether FocusLight Timer is currently running
unsigned long focusTimerStartTime = 0; // When the timer was started
unsigned long focusTimerElapsed = 0; // Elapsed time in milliseconds
const float MAX_FOCUS_TIMER_SECONDS = 120.0; // Maximum FocusLight Timer value in seconds
bool btn2PressHandled = false; // Flag to handle btn2 press once

// Exposure Timer functionality for btn6/btn7/btn8
float exposureTimerValue = 8.0f; // Initial value in seconds
const float MIN_EXPOSURE_TIMER_SECONDS = 0.1f;
const float MAX_EXPOSURE_TIMER_SECONDS = 999.9f;
const float EXPOSURE_TIMER_STEP = 0.1f;
bool exposureTimerRunning = false; // Is the exposure timer counting down?
bool exposureTimerPaused = false; // Is the exposure timer paused?
unsigned long exposureTimerStartTime = 0; // When countdown started or resumed
float exposureTimerElapsed = 0.0f; // Elapsed time in seconds (float for sub-second accuracy)
float exposureTimerCountdown = 0.0f; // Remaining time in seconds
bool btn6PressHandled = false; // For single press on btn6 (decrease)
bool btn7PressHandled = false; // For single press on btn7 (increase)
bool btn8PressHandled = false; // For single press on btn8 (start/pause/resume)

// Base Exposure & F-Stop Mode
float baseExposureValue = 0.0f;
bool baseExposureSet = false;
int fStopStepIndex = 0; // 0=1.000, 1=0.500, 2=0.333, 3=0.167, 4=0.083
const float F_STOP_STEPS[] = {1.0f, 0.5f, 0.333333f, 0.166667f, 0.083333f};
// We will calculate display values dynamically based on difference
bool btn3PressHandled = false;
bool btn4PressHandled = false;

// Step Display State
unsigned long stepDisplayStartTime = 0;
const unsigned long STEP_DISPLAY_DURATION = 1000;
bool stepDisplayActive = false;

// Buzzer timing state
unsigned long focusLastShortBeepSecond = 0;
unsigned long focusLastLongBeepSecond = 0;
unsigned long exposureLastLongBeepSecond = 0;
unsigned long exposureBeepAccumulatedMs = 0;

/*
  Apply brightness settings for display and LEDs.
  Range: 0 (dimmest) to 7 (brightest).
*/
void hwApplyBrightness(void) {
  tm.brightness(DISPLAY_LED_BRIGHTNESS);
}

// ============================================================================
// HARDWARE INTERFACE SECTION - Display, LEDs, Relay, and Button Control
// ============================================================================

/*
  Set display to show text (max 8 characters)
*/
void hwSetDisplay(const char* text) {
  tm.displayText(text);
}

/*
  Round a value to one decimal place for display
*/
float roundToTenth(float value) {
  if (value < 0.0f) return 0.0f;
  return ((int)(value * 10.0f + 0.5f)) / 10.0f;
}

/*
  Format a right-aligned decimal value for TM1638 displayText.
  Ensures 8 visible positions after dot-removal handling.
*/
void formatRightAlignedDecimal(char* out, size_t size, float value) {
  char numBuf[8];
  snprintf(numBuf, sizeof(numBuf), "%4.1f", value);

  size_t len = strlen(numBuf);
  size_t dotCount = 0;
  for (size_t i = 0; i < len; i++) {
    if (numBuf[i] == '.') dotCount++;
  }

  int effectiveLen = (int)len - (int)dotCount;
  int padLeft = 8 - effectiveLen;
  if (padLeft < 0) padLeft = 0;

  size_t idx = 0;
  while (idx < (size_t)padLeft && idx < size - 1) {
    out[idx++] = ' ';
  }
  for (size_t i = 0; i < len && idx < size - 1; i++) {
    out[idx++] = numBuf[i];
  }
  out[idx] = '\0';
}

/*
  Format text for TM1638 displayText where '.' does not take a character slot.
  Ensures exactly 8 visible characters (dots excluded) and preserves dots.
*/
void formatTextWithDots(char* out, size_t size, const char* text) {
  size_t idx = 0;
  size_t effectiveLen = 0;

  for (size_t i = 0; text[i] != '\0' && idx < size - 1; i++) {
    char c = text[i];
    if (c == '.') {
      if (idx > 0 && effectiveLen > 0 && effectiveLen <= 8 && idx < size - 1) {
        out[idx++] = c;
      }
      continue;
    }

    if (effectiveLen >= 8) break;
    out[idx++] = c;
    effectiveLen++;
  }

  while (effectiveLen < 8 && idx < size - 1) {
    out[idx++] = ' ';
    effectiveLen++;
  }

  out[idx] = '\0';
}

/*
  General display helper that handles dot-aware formatting and padding
*/
void hwDisplayText(const char* text) {
  char displayBuffer[16];
  formatTextWithDots(displayBuffer, sizeof(displayBuffer), text);
  hwSetDisplay(displayBuffer);
}

/*
  Set LED state (0 = off, 1 = on)
  position: 0-7 for LEDs 1-8
  value: 0 = off, 1 = on
*/
void hwSetLED(uint8_t position, uint8_t value) {
  tm.setLED(position, value);
}

/*
  Set multiple LEDs at once
  ledPattern: each bit represents one LED (bit 0 = LED1, bit 7 = LED8)
*/
void hwSetLEDs(uint8_t ledPattern) {
  tm.setLEDs(ledPattern);
}

/*
  Turn relay on or off
  state: true = ON, false = OFF
*/
void hwSetRelay(bool state) {
  digitalWrite(RELAY_PIN, state ? HIGH : LOW);
}

/*
  Play a buzzer tone
  durationMs: duration in milliseconds
*/
void hwBeepShort(void) {
  tone(BUZZER_PIN, BUZZER_SHORT_FREQUENCY, BUZZER_SHORT_BEEP_MS);
}

void hwBeepLong(void) {
  tone(BUZZER_PIN, BUZZER_LONG_FREQUENCY, BUZZER_LONG_BEEP_MS);
}

/*
  Read current button state
  Returns: 0 if no button pressed, or button value (1, 2, 4, 8, 16, 32, 64, 128)
*/
uint8_t hwReadButton(void) {
  return tm.readButtons();
}

/*
  Check if a button is being continuously pressed
  duration: Duration in milliseconds the button needs to be held
  Returns: true if button has been held for at least 'duration' ms
*/
bool hwCheckContinuousPress(uint8_t buttonValue, unsigned long duration) {
  if (buttonValue == 0) {
    // No button pressed, reset
    continuousPressDetected = false;
    buttonPressStartTime = 0;
    return false;
  }
  
  if (buttonValue != lastButton) {
    // New button press, start timer
    buttonPressStartTime = millis();
    continuousPressDetected = false;
    return false;
  }
  
  // Same button still pressed, check if threshold reached
  if (!continuousPressDetected && millis() - buttonPressStartTime >= duration) {
    continuousPressDetected = true;
    return true;
  }
  
  return continuousPressDetected;
}



/*
  Start the FocusLight Timer
*/
void focusTimerStart(void) {
  focusTimerRunning = true;
  focusTimerStartTime = millis();
  focusLastShortBeepSecond = 0;
  focusLastLongBeepSecond = 0;
  hwSetRelay(true);
  Serial.println("FocusLight Timer started");
}

/*
  Stop the FocusLight Timer
*/
void focusTimerStop(void) {
  focusTimerRunning = false;
  hwSetRelay(false);
  focusLastShortBeepSecond = 0;
  focusLastLongBeepSecond = 0;
  Serial.println("FocusLight Timer stopped");
}

/*
  Clear the FocusLight Timer and display
*/
void focusTimerClear(void) {
  focusTimerRunning = false;
  focusTimerElapsed = 0;
  hwSetRelay(false);
  focusLastShortBeepSecond = 0;
  focusLastLongBeepSecond = 0;
  setNormalState();
  Serial.println("FocusLight Timer cleared");
}

/*
  Update and display the FocusLight Timer
  Returns elapsed time in seconds (as float)
*/
float focusTimerUpdate(void) {
  if (focusTimerRunning) {
    focusTimerElapsed = millis() - focusTimerStartTime;
    hwSetLED(1, 1); // LED2 ON (position 1)
  }

  float secs = focusTimerElapsed / 1000.0f;

  // Check if max time reached
  if (secs >= MAX_FOCUS_TIMER_SECONDS) {
    secs = MAX_FOCUS_TIMER_SECONDS;
    focusTimerStop();
  }

  // Format display: "FOC   12.6" (FOC left, timer right, moved two steps right)
  char displayBuffer[9];
  // Two spaces after 'FOC ', then timer right-justified in 4.1f format
  float displaySecs = roundToTenth(secs);
  sprintf(displayBuffer, "FOC  %4.1f", displaySecs);
  hwDisplayText(displayBuffer);

  // If timer is not running, turn off LED2 and restore default display
  if (!focusTimerRunning) {
    hwSetLED(1, 0); // LED2 OFF (position 1)
    setNormalState();
  }

  return secs;
}

/*
  Buzzer updates for FocusLight Timer
  - Short beep every second
  - Long beep every 10 seconds
*/
void focusTimerBuzzerUpdate(void) {
  if (!focusTimerRunning) return;

  unsigned long elapsedSeconds = focusTimerElapsed / 1000;
  if (elapsedSeconds == 0) return;

  if (elapsedSeconds % 10 == 0 && elapsedSeconds != focusLastLongBeepSecond) {
    hwBeepLong();
    focusLastLongBeepSecond = elapsedSeconds;
    focusLastShortBeepSecond = elapsedSeconds; // Avoid double beep on 10s
  } else if (elapsedSeconds != focusLastShortBeepSecond) {
    hwBeepShort();
    focusLastShortBeepSecond = elapsedSeconds;
  }
}

/*
  Buzzer updates for Exposure Timer
  - Long beep every 10 seconds
*/
void exposureTimerBuzzerUpdate(void) {
  if (!exposureTimerRunning) return;

  unsigned long totalElapsedMs = exposureBeepAccumulatedMs + (millis() - exposureTimerStartTime);
  unsigned long elapsedSeconds = totalElapsedMs / 1000;
  if (elapsedSeconds == 0) return;

  if (elapsedSeconds % 10 == 0 && elapsedSeconds != exposureLastLongBeepSecond) {
    hwBeepLong();
    exposureLastLongBeepSecond = elapsedSeconds;
  }
}

// Set the normal state: display exposureTimer value, relay off, all LEDs off (except F-Stop mode indicators)
void setNormalState() {
  hwSetRelay(false);
  
  // Set LEDs based on mode
  if (baseExposureSet) {
    // Determine which LED to light up based on F-Stop Step
    // LED3 (pos 2) to LED7 (pos 6)
    // fStopStepIndex: 0->LED3, 1->LED4, 2->LED5, 3->LED6, 4->LED7
    uint8_t ledPattern = 0;
    if (fStopStepIndex >= 0 && fStopStepIndex <= 4) {
      ledPattern = (1 << (fStopStepIndex + 2));
    }
    hwSetLEDs(ledPattern);
  } else {
    hwSetLEDs(0x00);
  }

  char finalDisplay[17]; // Buffer for final display string
  
  if (stepDisplayActive) {
      // Show "STEP X.XX" briefly (e.g. STEP 1.00, STEP 0.33)
      snprintf(finalDisplay, sizeof(finalDisplay), "STEP %.2f", F_STOP_STEPS[fStopStepIndex]);
      hwDisplayText(finalDisplay);
      return; 
  }
  
  // Format the time part (right side)
  float displayExposure = roundToTenth(exposureTimerValue);
  char timePart[10];
  snprintf(timePart, sizeof(timePart), "%.1f", displayExposure);

  // Determine what to show on the left
  char leftPart[10] = "";
  bool showSplit = false;

  if (baseExposureSet) {
    // Show Base/Diff
    showSplit = true;
    // Check if exposure equals base exposure (within small epsilon)
    if (fabs(exposureTimerValue - baseExposureValue) < 0.001f) {
      strcpy(leftPart, "BASE");
    } else {
      // Calculate f-stop difference
      // f-stops = log2(current / base)
      // log2(x) = ln(x) / ln(2)
      if (baseExposureValue > 0) {
        float fStops = log10(exposureTimerValue / baseExposureValue) / log10(2.0f);
        // Use precision 2 with space flag for positive numbers to ensure alignment
        snprintf(leftPart, sizeof(leftPart), "% .2f", fStops);
      } else {
        strcpy(leftPart, "ERR");
      }
    }
  }

  if (showSplit) {
    // Combine left and time parts with alignment
    // Calculate visible lengths (ignoring '.' characters)
    int leftVis = 0;
    for(int i=0; leftPart[i]; i++) if(leftPart[i] != '.') leftVis++;
    
    int timeVis = 0;
    for(int i=0; timePart[i]; i++) if(timePart[i] != '.') timeVis++;
    
    int spaces = 8 - leftVis - timeVis;
    if (spaces < 1) spaces = 1; // Ensure at least one space
    
    // Construct final string
    strcpy(finalDisplay, leftPart);
    size_t currentLen = strlen(finalDisplay);
    for(int i = 0; i < spaces && currentLen < sizeof(finalDisplay) - 1; i++) {
        finalDisplay[currentLen++] = ' ';
    }
    finalDisplay[currentLen] = '\0';
    
    // Check if we have room to concatenate timePart
    if (currentLen + strlen(timePart) < sizeof(finalDisplay)) {
        strcat(finalDisplay, timePart);
    }
    
    hwDisplayText(finalDisplay);

  } else {
    // Normal mode: just Right Aligned time
    formatRightAlignedDecimal(finalDisplay, sizeof(finalDisplay), displayExposure);
    hwDisplayText(finalDisplay);
  }
}

// ============================================================================
// END HARDWARE INTERFACE SECTION
// ============================================================================

void setup() {
  // Initialize serial for debugging
  Serial.begin(9600);
  delay(100);
  
  // Initialize the TM1638plus display
  tm.displayBegin();
  hwApplyBrightness();
  
  // Initialize relay pin as output and turn it off
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  hwSetRelay(false);

  // Startup: all segments and LEDs on briefly
  hwDisplayText("8.8.8.8.8.8.8.8.");
  hwSetLEDs(0xFF);
  delay(STARTUP_ALL_ON_MS);
  hwDisplayText("        ");
  hwSetLEDs(0x00);

  // Show app version on startup (padded to clear display)
  hwDisplayText(APP_VERSION);
  delay(VERSION_DISPLAY_MS);
  
  // Initialize display
  setNormalState();
  
  // Turn off all LEDs
  hwSetLEDs(0x00);
  
  Serial.println("Button LED Test Started");
  Serial.println("Button 2: Turn relay ON");
  Serial.println("Button 3: Turn relay OFF");
}

void loop() {
  // Read button state
  uint8_t buttons = hwReadButton();
  
  // Check for continuous press detection (300 ms threshold)
  bool isContinuousPress = hwCheckContinuousPress(buttons, CONTINUOUS_PRESS_THRESHOLD);

  // ============================================================================
  // BUTTON HANDLING SECTION
  // ============================================================================
  
  switch (buttons) {
    
    // ===== BTN1: Cancel all timers and return to normal state =====
    case btn1:
      if (!btn1PressHandled) {
        btn1PressHandled = true;
        bool anyCancelled = false;
        
        // Stop FocusLight Timer if running
        if (focusTimerRunning || focusTimerElapsed > 0) {
          focusTimerRunning = false;
          focusTimerElapsed = 0;
          hwSetRelay(false);
          anyCancelled = true;
          Serial.println("FocusLight Timer cancelled");
        }
        // Stop Exposure Timer if running or paused
        if (exposureTimerRunning || exposureTimerPaused) {
          exposureTimerRunning = false;
          exposureTimerPaused = false;
          exposureTimerCountdown = 0.0f;
          exposureBeepAccumulatedMs = 0;
          exposureLastLongBeepSecond = 0;
          hwSetRelay(false);
          anyCancelled = true;
          Serial.println("Exposure Timer cancelled");
        }
        
        // If anything was cancelled, turn off LEDs immediately, show Cancel, then normal state
        if (anyCancelled) {
          hwSetLEDs(0x00);  // Turn off all LEDs immediately
          hwDisplayText(" CANCEL ");
          delay(500);
          setNormalState();
        }
      }
      break;
    
    // ===== BTN2: FocusLight Timer (continuous press to run, short press to toggle) =====
    case btn2:
      if (exposureTimerRunning) {
        break; // Exposure timer running: ignore btn2
      }
      if (isContinuousPress) {
        // Continuous press: Start timer and keep it running while pressed
        if (!focusTimerRunning) {
          if (exposureTimerPaused) {
            exposureTimerPaused = false;
            exposureTimerCountdown = 0.0f;
            exposureBeepAccumulatedMs = 0;
            exposureLastLongBeepSecond = 0;
            hwSetRelay(false);
            hwSetLED(7, 0);  // LED8 OFF
          }
          focusTimerStart();
        }
        focusTimerUpdate();
      } else if (!btn2PressHandled) {
        // Short press: Toggle timer (start or clear)
        btn2PressHandled = true;
        if (focusTimerRunning || focusTimerElapsed > 0) {
          focusTimerClear();
        } else {
          if (exposureTimerRunning || exposureTimerPaused) {
            exposureTimerRunning = false;
            exposureTimerPaused = false;
            exposureTimerCountdown = 0.0f;
            exposureBeepAccumulatedMs = 0;
            exposureLastLongBeepSecond = 0;
            hwSetRelay(false);
          }
          focusTimerStart();
          focusTimerUpdate();
        }
      }
      break;
    
    // ===== BTN3: Set/Unset Base Exposure =====
    case btn3:
      if (focusTimerRunning || exposureTimerRunning) break; // Inactive during timer run
      
      if (!btn3PressHandled) {
        btn3PressHandled = true;
        
        if (baseExposureSet) {
          // Toggle OFF
          baseExposureSet = false;
          // Preserve fStopStepIndex for next use
        } else {
          // Toggle ON
          baseExposureSet = true;
          baseExposureValue = exposureTimerValue;
          // Reuse existing fStopStepIndex
        }
        setNormalState();
      }
      break;

    // ===== BTN4: Cycle F-Stop Steps =====
    case btn4:
      // Cycle steps for F-Stop mode
      // Only active if F-Stop Mode (Base Exposure) is set
      if (focusTimerRunning || exposureTimerRunning) break;

      if (!btn4PressHandled) {
          btn4PressHandled = true;
          
          if (baseExposureSet) {
            fStopStepIndex++;
            if (fStopStepIndex > 4) fStopStepIndex = 0;
            
            // Trigger temporary display of the selected step
            stepDisplayActive = true;
            stepDisplayStartTime = millis();
            
            setNormalState(); // Update display immediately
          }
      }
      break;

    // ===== BTN5: Unused =====
    case btn5:
      break;
    
    // ===== BTN6: Decrease exposureTimer value (with auto-repeat) =====
    case btn6:
      if (focusTimerRunning || exposureTimerRunning) {
        break; // Ignore when any timer is running
      }
      
      // Stop temporary step display if it's active so user sees effect immediately
      if (stepDisplayActive) {
         stepDisplayActive = false;
         setNormalState();
      }

      if (!exposureTimerRunning && !exposureTimerPaused) {
        bool shouldDecrement = false;
        
        // First press
        if (!btn6PressHandled) {
          btn6PressHandled = true;
          shouldDecrement = true;
          lastAutoRepeatTime = millis();
        }
        // Auto-repeat after threshold (use fast interval after prolonged press)
        else if (isContinuousPress) {
          unsigned long holdDuration = millis() - buttonPressStartTime;
          unsigned long repeatInterval = (holdDuration >= AUTO_REPEAT_ULTRA_THRESHOLD)
            ? AUTO_REPEAT_INTERVAL_ULTRA
            : (holdDuration >= AUTO_REPEAT_FAST_THRESHOLD)
              ? AUTO_REPEAT_INTERVAL_FAST
              : AUTO_REPEAT_INTERVAL;
          if (millis() - lastAutoRepeatTime >= repeatInterval) {
            shouldDecrement = true;
            lastAutoRepeatTime = millis();
          }
        }
        
        if (shouldDecrement) {
          if (baseExposureSet) {
             // F-Stop Mode: decrease by fStopStep
             float steps = F_STOP_STEPS[fStopStepIndex];
             // Math: NewTime = OldTime / 2^(steps)  (for decrease)
             // Formula: t1 = t0 * 2^steps. Decrease means multiply by 2^(-steps)
             exposureTimerValue = exposureTimerValue * pow(2.0f, -steps);
          } else {
             // Seconds Mode: decrease by 0.1s
             exposureTimerValue -= EXPOSURE_TIMER_STEP;
          }

          if (exposureTimerValue < MIN_EXPOSURE_TIMER_SECONDS) {
            exposureTimerValue = MIN_EXPOSURE_TIMER_SECONDS;
          }
          setNormalState(); // Update display (centralized)
        }
      }
      break;
    
    // ===== BTN7: Increase exposureTimer value (with auto-repeat) =====
    case btn7:
      if (focusTimerRunning || exposureTimerRunning) {
        break; // Ignore when any timer is running
      }
      
      // Stop temporary step display if it's active so user sees effect immediately
      if (stepDisplayActive) {
         stepDisplayActive = false;
         setNormalState();
      }

      if (!exposureTimerRunning && !exposureTimerPaused) {
        bool shouldIncrement = false;
        
        // First press
        if (!btn7PressHandled) {
          btn7PressHandled = true;
          shouldIncrement = true;
          lastAutoRepeatTime = millis();
        }
        // Auto-repeat after threshold (use fast interval after prolonged press)
        else if (isContinuousPress) {
          unsigned long holdDuration = millis() - buttonPressStartTime;
          unsigned long repeatInterval = (holdDuration >= AUTO_REPEAT_ULTRA_THRESHOLD)
            ? AUTO_REPEAT_INTERVAL_ULTRA
            : (holdDuration >= AUTO_REPEAT_FAST_THRESHOLD)
              ? AUTO_REPEAT_INTERVAL_FAST
              : AUTO_REPEAT_INTERVAL;
          if (millis() - lastAutoRepeatTime >= repeatInterval) {
            shouldIncrement = true;
            lastAutoRepeatTime = millis();
          }
        }
        
        if (shouldIncrement) {
          if (baseExposureSet) {
             // F-Stop Mode: increase by fStopStep
             float steps = F_STOP_STEPS[fStopStepIndex];
             // Math: NewTime = OldTime * 2^(steps)
             exposureTimerValue = exposureTimerValue * pow(2.0f, steps);
          } else {
             // Seconds Mode: increase by 0.1s
             exposureTimerValue += EXPOSURE_TIMER_STEP;
          }

          if (exposureTimerValue > MAX_EXPOSURE_TIMER_SECONDS) {
            exposureTimerValue = MAX_EXPOSURE_TIMER_SECONDS;
          }
           setNormalState(); // Update display (centralized)
        }
      }
      break;
    
    // ===== BTN8: Start/Pause/Resume exposureTimer countdown =====
    case btn8:
      if (focusTimerRunning) {
        break; // FocusLight timer running: ignore btn8
      }
      
      // Stop temporary step display
      if (stepDisplayActive) {
         stepDisplayActive = false;
      }

      if (!btn8PressHandled) {
        btn8PressHandled = true;
        if (!exposureTimerRunning && !exposureTimerPaused) {
          // Start exposure timer countdown
          if (focusTimerRunning || focusTimerElapsed > 0) {
            focusTimerClear();
          }
          exposureTimerRunning = true;
          exposureTimerPaused = false;
          exposureTimerCountdown = exposureTimerValue;
          exposureBeepAccumulatedMs = 0;
          exposureLastLongBeepSecond = 0;
          exposureTimerStartTime = millis();
          hwSetRelay(true);
          hwSetLED(7, 1);  // LED8 ON (position 7)
          char expDisp8[10];
          float displayCountdown8 = roundToTenth(exposureTimerCountdown);
          formatRightAlignedDecimal(expDisp8, sizeof(expDisp8), displayCountdown8);
          hwDisplayText(expDisp8);
        } else if (exposureTimerRunning) {
          // Pause exposure timer
          exposureTimerRunning = false;
          exposureTimerPaused = true;
          unsigned long elapsed = millis() - exposureTimerStartTime;
          exposureBeepAccumulatedMs += elapsed;
          exposureTimerCountdown -= (float)elapsed / 1000.0f;
          if (exposureTimerCountdown < 0.0f) exposureTimerCountdown = 0.0f;
          hwSetRelay(false);
          hwSetLED(7, 0);  // LED8 OFF (position 7)
          char expDisp8[10];
          float displayCountdown8 = roundToTenth(exposureTimerCountdown);
          formatRightAlignedDecimal(expDisp8, sizeof(expDisp8), displayCountdown8);
          hwDisplayText(expDisp8);
        } else if (exposureTimerPaused) {
          // Resume exposure timer
          exposureTimerRunning = true;
          exposureTimerPaused = false;
          exposureTimerStartTime = millis();
          hwSetRelay(true);
          hwSetLED(7, 1);  // LED8 ON (position 7)
          char expDisp8[10];
          float displayCountdown8 = roundToTenth(exposureTimerCountdown);
          formatRightAlignedDecimal(expDisp8, sizeof(expDisp8), displayCountdown8);
          hwDisplayText(expDisp8);
        }
      }
      break;
    
    // ===== NO BUTTON PRESSED =====
    default:
      // Reset all button press handlers
      btn1PressHandled = false;
      btn2PressHandled = false;
      btn3PressHandled = false;
      btn4PressHandled = false;
      btn6PressHandled = false;
      btn7PressHandled = false;
      btn8PressHandled = false;
      
      // Handle btn2 release (FocusLight Timer)
      if (lastButton == btn2 && focusTimerRunning && isContinuousPress) {
        // Stop timer from continuous press and restore normal state
        focusTimerStop();
        hwSetLED(1, 0);
        setNormalState();
      } else if (lastButton == btn2 && focusTimerRunning && focusTimerElapsed > 0) {
        // Stop timer from short press but keep displaying time
        focusTimerStop();
        focusTimerUpdate();
      }
      break;
  }
  
  // ============================================================================
  // TIMER UPDATE SECTION (runs every loop iteration)
  // ============================================================================
  
  // Update FocusLight Timer display if running (and not being handled by btn2)
  if (focusTimerRunning && buttons != btn2) {
    focusTimerUpdate();
  }

  // Handle temporary step display timeout
  if (stepDisplayActive) {
      if (millis() - stepDisplayStartTime > STEP_DISPLAY_DURATION) {
          stepDisplayActive = false;
          // Refresh display if not running exposure timer
          if (!exposureTimerRunning && !focusTimerRunning && !exposureTimerPaused) {
             setNormalState();
          }
      }
  }
  
  // Update Exposure Timer countdown
  if (exposureTimerRunning) {
    unsigned long elapsed = millis() - exposureTimerStartTime;
    float remaining = exposureTimerCountdown - (float)elapsed / 1000.0f;
    if (remaining <= 0.0f) {
      // Timer finished
      exposureTimerRunning = false;
      exposureTimerPaused = false;
      exposureTimerCountdown = 0.0f;
      exposureBeepAccumulatedMs = 0;
      exposureLastLongBeepSecond = 0;
      hwSetRelay(false);
      hwSetLED(7, 0);  // LED8 OFF (position 7)
      setNormalState();
    } else {
      // Update display with remaining time
      char expDisp[10];
      float displayRemaining = roundToTenth(remaining);
      formatRightAlignedDecimal(expDisp, sizeof(expDisp), displayRemaining);
      hwDisplayText(expDisp);
    }
  }

  // Update buzzer patterns
  focusTimerBuzzerUpdate();
  exposureTimerBuzzerUpdate();
  
  // ============================================================================
  // STATE TRACKING
  // ============================================================================
  
  // Track last button for release detection
  lastButton = buttons;
  
  delay(50); // Small delay to prevent overwhelming the loop
}
