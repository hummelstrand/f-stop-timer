void setNormalState();

/*
  F-Stop Timer
  
  A darkroom enlarger timer with FocusLight and Exposure Timer modes.
  
  Hardware: TM1638plus module with 8 buttons, 8 LEDs, relay, buzzer
  Uses the existing TM1638plus library
*/

#include <TM1638plus.h>

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
const unsigned long AUTO_REPEAT_FAST_THRESHOLD = 2000; // milliseconds before switching to fast repeat
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
  hwSetRelay(true);
  Serial.println("FocusLight Timer started");
}

/*
  Stop the FocusLight Timer
*/
void focusTimerStop(void) {
  focusTimerRunning = false;
  hwSetRelay(false);
  Serial.println("FocusLight Timer stopped");
}

/*
  Clear the FocusLight Timer and display
*/
void focusTimerClear(void) {
  focusTimerRunning = false;
  focusTimerElapsed = 0;
  hwSetRelay(false);
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
  sprintf(displayBuffer, "FOC  %4.1f", secs);
  hwSetDisplay(displayBuffer);

  // If timer is not running, turn off LED2 and restore default display
  if (!focusTimerRunning) {
    hwSetLED(1, 0); // LED2 OFF (position 1)
    setNormalState();
  }

  return secs;
}

// Set the normal state: display exposureTimer value, relay off, all LEDs off
void setNormalState() {
  hwSetRelay(false);
  hwSetLEDs(0x00);
  char expDisp[9];
  sprintf(expDisp, "    %4.1f", exposureTimerValue);
  hwSetDisplay(expDisp);
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
  
  // Initialize relay pin as output and turn it off
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  hwSetRelay(false);
  
  // Initialize display with "start" text
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
          hwSetRelay(false);
          anyCancelled = true;
          Serial.println("Exposure Timer cancelled");
        }
        
        // If anything was cancelled, turn off LEDs immediately, show Cancel, then normal state
        if (anyCancelled) {
          hwSetLEDs(0x00);  // Turn off all LEDs immediately
          hwSetDisplay(" CANCEL ");
          delay(500);
          setNormalState();
        }
      }
      break;
    
    // ===== BTN2: FocusLight Timer (continuous press to run, short press to toggle) =====
    case btn2:
      if (isContinuousPress) {
        // Continuous press: Start timer and keep it running while pressed
        if (!focusTimerRunning) {
          focusTimerStart();
        }
        focusTimerUpdate();
      } else if (!btn2PressHandled) {
        // Short press: Toggle timer (start or clear)
        btn2PressHandled = true;
        if (focusTimerRunning || focusTimerElapsed > 0) {
          focusTimerClear();
        } else {
          focusTimerStart();
          focusTimerUpdate();
        }
      }
      break;
    
    // ===== BTN6: Decrease exposureTimer value (with auto-repeat) =====
    case btn6:
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
          unsigned long repeatInterval = (holdDuration >= AUTO_REPEAT_FAST_THRESHOLD) ? AUTO_REPEAT_INTERVAL_FAST : AUTO_REPEAT_INTERVAL;
          if (millis() - lastAutoRepeatTime >= repeatInterval) {
            shouldDecrement = true;
            lastAutoRepeatTime = millis();
          }
        }
        
        if (shouldDecrement) {
          exposureTimerValue -= EXPOSURE_TIMER_STEP;
          if (exposureTimerValue < MIN_EXPOSURE_TIMER_SECONDS) {
            exposureTimerValue = MIN_EXPOSURE_TIMER_SECONDS;
          }
          char expDisp6[9];
          sprintf(expDisp6, "    %4.1f", exposureTimerValue);
          hwSetDisplay(expDisp6);
        }
      }
      break;
    
    // ===== BTN7: Increase exposureTimer value (with auto-repeat) =====
    case btn7:
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
          unsigned long repeatInterval = (holdDuration >= AUTO_REPEAT_FAST_THRESHOLD) ? AUTO_REPEAT_INTERVAL_FAST : AUTO_REPEAT_INTERVAL;
          if (millis() - lastAutoRepeatTime >= repeatInterval) {
            shouldIncrement = true;
            lastAutoRepeatTime = millis();
          }
        }
        
        if (shouldIncrement) {
          exposureTimerValue += EXPOSURE_TIMER_STEP;
          if (exposureTimerValue > MAX_EXPOSURE_TIMER_SECONDS) {
            exposureTimerValue = MAX_EXPOSURE_TIMER_SECONDS;
          }
          char expDisp7[9];
          sprintf(expDisp7, "    %4.1f", exposureTimerValue);
          hwSetDisplay(expDisp7);
        }
      }
      break;
    
    // ===== BTN8: Start/Pause/Resume exposureTimer countdown =====
    case btn8:
      if (!btn8PressHandled) {
        btn8PressHandled = true;
        if (!exposureTimerRunning && !exposureTimerPaused) {
          // Start exposure timer countdown
          exposureTimerRunning = true;
          exposureTimerPaused = false;
          exposureTimerCountdown = exposureTimerValue;
          exposureTimerStartTime = millis();
          hwSetRelay(true);
          hwSetLED(7, 1);  // LED8 ON (position 7)
          char expDisp8[9];
          sprintf(expDisp8, "    %4.1f", exposureTimerCountdown);
          hwSetDisplay(expDisp8);
        } else if (exposureTimerRunning) {
          // Pause exposure timer
          exposureTimerRunning = false;
          exposureTimerPaused = true;
          unsigned long elapsed = millis() - exposureTimerStartTime;
          exposureTimerCountdown -= (float)elapsed / 1000.0f;
          if (exposureTimerCountdown < 0.0f) exposureTimerCountdown = 0.0f;
          hwSetRelay(false);
          hwSetLED(7, 0);  // LED8 OFF (position 7)
          char expDisp8[9];
          sprintf(expDisp8, "    %4.1f", exposureTimerCountdown);
          hwSetDisplay(expDisp8);
        } else if (exposureTimerPaused) {
          // Resume exposure timer
          exposureTimerRunning = true;
          exposureTimerPaused = false;
          exposureTimerStartTime = millis();
          hwSetRelay(true);
          hwSetLED(7, 1);  // LED8 ON (position 7)
          char expDisp8[9];
          sprintf(expDisp8, "    %4.1f", exposureTimerCountdown);
          hwSetDisplay(expDisp8);
        }
      }
      break;
    
    // ===== NO BUTTON PRESSED =====
    default:
      // Reset all button press handlers
      btn1PressHandled = false;
      btn2PressHandled = false;
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
  
  // Update Exposure Timer countdown
  if (exposureTimerRunning) {
    unsigned long elapsed = millis() - exposureTimerStartTime;
    float remaining = exposureTimerCountdown - (float)elapsed / 1000.0f;
    if (remaining <= 0.0f) {
      // Timer finished
      exposureTimerRunning = false;
      exposureTimerPaused = false;
      exposureTimerCountdown = 0.0f;
      hwSetRelay(false);
      hwSetLED(7, 0);  // LED8 OFF (position 7)
      setNormalState();
    } else {
      // Update display with remaining time
      char expDisp[9];
      sprintf(expDisp, "    %4.1f", remaining);
      hwSetDisplay(expDisp);
    }
  }
  
  // ============================================================================
  // STATE TRACKING
  // ============================================================================
  
  // Track last button for release detection
  lastButton = buttons;
  
  delay(50); // Small delay to prevent overwhelming the loop
}
