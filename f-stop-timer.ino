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
#else
  #error "Unsupported target: compile this sketch for an ESP8266 board (or AVR), not UNO R4 Renesas"
#endif

#define RELAY_PIN 5 // Relay control pin
#define BUZZER_PIN 16 // Buzzer control pin

// Rotary encoder 1 (exposure adjustment)
// Wiring uses GPIO4 (A), GPIO3 (B, labeled RX). Active-low with pull-ups.
#define ENC1_A_PIN 4   // Encoder 1 CLK/A
#define ENC1_B_PIN 3   // Encoder 1 DT/B (Labeled RX on the board)
// Encoder 1 tuning
#define ENCODER1_INVERT_DIRECTION false // Set true to swap CW/CCW behavior
const unsigned long ENCODER1_DEBOUNCE_MS = 2; // Minimum time between state changes

// Encoder 1 acceleration tuning (based on time between detents)
// Typical values: 120 ms (slow), 60 ms (medium), 30 ms (fast)
// If you want a more/less aggressive feel, raise or lower the thresholds or
// change the multipliers. Smaller thresholds = harder to trigger acceleration.
const unsigned long ENCODER1_ACCEL_FAST_MS = 60;
const unsigned long ENCODER1_ACCEL_ULTRA_MS = 30;
const uint8_t ENCODER1_ACCEL_FAST_MULT = 4;
const uint8_t ENCODER1_ACCEL_ULTRA_MULT = 10;

// App version
const char APP_VERSION[] = "VERS 0.9.2";
const unsigned long VERSION_DISPLAY_MS = 1100;
const unsigned long STARTUP_ALL_ON_MS = 400;
const unsigned long IO_DIAG_SEGMENT_HOLD_MS = 1000;
const unsigned long IO_DIAG_ENCODER_INDICATOR_MS = 250;

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
int fStopStepIndex = 0; // 0=1.000, 1=0.500, 2=0.333, 3=0.250, 4=0.167
const float F_STOP_STEPS[] = {1.0f, 0.5f, 0.333333f, 0.25f, 0.166667f};
// We will calculate display values dynamically based on difference
bool btn3PressHandled = false;
bool btn4PressHandled = false;

// Step Display State
unsigned long stepDisplayStartTime = 0;
const unsigned long STEP_DISPLAY_DURATION = 1000;
bool stepDisplayActive = false;

// Input/Output diagnostic mode (hold btn8 during startup to enter)
bool ioDiagModeActive = false;
unsigned long ioDiagModeStartMs = 0;
int8_t ioDiagLastEncoderDirection = 0;
unsigned long ioDiagLastEncoderDirectionMs = 0;

// Buzzer timing state
unsigned long focusLastLongBeepSecond = 0;
unsigned long exposureLastLongBeepSecond = 0;
unsigned long exposureBeepAccumulatedMs = 0;

// Rotary encoder 1 state tracking (exposure adjustment)
// We use a Gray-code transition table to decode direction and suppress bounce.
// Each read forms a 2-bit state: (A << 1) | B, where A/B are active-low.
// The 4-bit index (prev << 2) | curr maps to -1, 0, +1 step fragments.
// A full detent normally produces 4 fragments, so we accumulate until +/-4.
uint8_t encoder1PrevState = 0;
int8_t encoder1StepAccum = 0;
unsigned long encoder1LastTransitionMs = 0;
unsigned long encoder1LastDetentMs = 0;

bool hasSingleButtonPressed(uint8_t buttons, uint8_t &buttonNumber) {
  if (buttons == 0) return false;

  // Exactly one bit set => a single button press
  if ((buttons & (buttons - 1)) != 0) return false;

  for (uint8_t i = 0; i < 8; i++) {
    if (buttons & (1 << i)) {
      buttonNumber = i + 1;
      return true;
    }
  }

  return false;
}

void updateIODiagnosticsMode() {
  // Keep outputs in a known safe state
  hwSetRelay(false);
  hwSetLEDs(0xFF);

  unsigned long nowMs = millis();

  // Show all segments for a short period, then switch to live diagnostics
  if (nowMs - ioDiagModeStartMs < IO_DIAG_SEGMENT_HOLD_MS) {
    hwDisplayText("8.8.8.8.8.8.8.8.");
    return;
  }

  uint8_t buttons = hwReadButton();
  int8_t encoderStep = readEncoder1Step();
  if (encoderStep != 0) {
    ioDiagLastEncoderDirection = (encoderStep > 0) ? 1 : -1;
    ioDiagLastEncoderDirectionMs = nowMs;
  }

  char buttonPart[6];
  uint8_t buttonNumber = 0;
  if (buttons == 0) {
    strcpy(buttonPart, "B0");
  } else if (hasSingleButtonPressed(buttons, buttonNumber)) {
    snprintf(buttonPart, sizeof(buttonPart), "B%u", buttonNumber);
  } else {
    strcpy(buttonPart, "BM");
  }

  const char* encoderPart = "E0";
  if (nowMs - ioDiagLastEncoderDirectionMs <= IO_DIAG_ENCODER_INDICATOR_MS) {
    if (ioDiagLastEncoderDirection > 0) encoderPart = "E+";
    else if (ioDiagLastEncoderDirection < 0) encoderPart = "E-";
  }

  char displayBuf[16];
  snprintf(displayBuf, sizeof(displayBuf), "%s %s", buttonPart, encoderPart);
  hwDisplayText(displayBuf);
}

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
    hwSetLED(0, 1); // LED1 ON (position 0)
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

  // If timer is not running, turn off LED1 and restore default display
  if (!focusTimerRunning) {
    hwSetLED(0, 0); // LED1 OFF (position 0)
    setNormalState();
  }

  return secs;
}

/*
  Buzzer updates for FocusLight Timer
  - Long beep every 10 seconds
*/
void focusTimerBuzzerUpdate(void) {
  if (!focusTimerRunning) return;

  unsigned long elapsedSeconds = focusTimerElapsed / 1000;
  if (elapsedSeconds == 0) return;

  if (elapsedSeconds % 10 == 0 && elapsedSeconds != focusLastLongBeepSecond) {
    hwBeepLong();
    focusLastLongBeepSecond = elapsedSeconds;
  }
}

/*
  Buzzer updates for Exposure Timer
  - Long beep every 10 seconds
*/
void exposureTimerBuzzerUpdate(void) {
  if (!exposureTimerRunning) return;

  unsigned long elapsedMs = millis() - exposureTimerStartTime;
  float remaining = exposureTimerCountdown - (float)elapsedMs / 1000.0f;
  
  if (remaining < 0) remaining = 0;
  
  // Round up to get the current "second block" we are in (e.g. 19.9s -> 20)
  unsigned long remainingSeconds = (unsigned long)ceil(remaining);
  
  if (remainingSeconds > 0 && remainingSeconds % 10 == 0 && remainingSeconds != exposureLastLongBeepSecond) {
    hwBeepLong();
    exposureLastLongBeepSecond = remainingSeconds;
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
    if (spaces < 0) spaces = 0; // Allow zero spaces if needed to fit
    
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

  // --------------------------------------------------------------------------
  // ROTARY ENCODER INPUT SETUP
  // The encoders are passive mechanical devices. Their A/B contacts connect
  // the input pin to GND when activated. When released, the inputs would
  // otherwise float (random HIGH/LOW) and cause false triggers.
  //
  // INPUT_PULLUP enables the ESP8266's internal pull-up resistor so each pin
  // is held HIGH (3.3V) by default and reads LOW only when the contact closes.
  // This avoids extra external resistors for the encoder signals.
  pinMode(ENC1_A_PIN, INPUT_PULLUP);
  pinMode(ENC1_B_PIN, INPUT_PULLUP);
  // --------------------------------------------------------------------------

  // Initialize encoder 1 state to the current pin levels so the first read
  // does not generate a false movement event.
  uint8_t a1Init = (digitalRead(ENC1_A_PIN) == LOW) ? 1 : 0;
  uint8_t b1Init = (digitalRead(ENC1_B_PIN) == LOW) ? 1 : 0;
  encoder1PrevState = (a1Init << 1) | b1Init;

  // Optional Input/Output diagnostic mode:
  // Hold btn8 during boot to keep all LEDs on and show input diagnostics.
  uint8_t startupButtons = hwReadButton();
  if ((startupButtons & btn8) != 0) {
    ioDiagModeActive = true;
    ioDiagModeStartMs = millis();
    ioDiagLastEncoderDirection = 0;
    ioDiagLastEncoderDirectionMs = 0;
    hwSetLEDs(0xFF);
    hwDisplayText("8.8.8.8.8.8.8.8.");
    Serial.println("Input/Output diagnostic mode active (btn8 held at startup)");
    return;
  }

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

// ============================================================================
// HELPER FUNCTIONS TO SIMPLIFY COMPLICATED LOGIC
// ============================================================================

// Helper to display a time value right-aligned
void displayTime(float timeVal) {
  char buf[16];
  formatRightAlignedDecimal(buf, sizeof(buf), roundToTenth(timeVal));
  hwDisplayText(buf);
}

// Logic for increasing/decreasing exposure time (btn6/btn7)
void handleExposureChange(bool increase, bool isContinuousPress, bool &pressHandled) {
  bool shouldChange = false;
  
  // First press
  if (!pressHandled) {
    pressHandled = true;
    shouldChange = true;
    lastAutoRepeatTime = millis();
  }
  // Auto-repeat logic
  else if (isContinuousPress) {
    unsigned long holdDuration = millis() - buttonPressStartTime;
    unsigned long repeatInterval = (holdDuration >= AUTO_REPEAT_ULTRA_THRESHOLD)
      ? AUTO_REPEAT_INTERVAL_ULTRA
      : (holdDuration >= AUTO_REPEAT_FAST_THRESHOLD)
        ? AUTO_REPEAT_INTERVAL_FAST
        : AUTO_REPEAT_INTERVAL;
        
    if (millis() - lastAutoRepeatTime >= repeatInterval) {
      shouldChange = true;
      lastAutoRepeatTime = millis();
    }
  }

  if (shouldChange) {
    if (baseExposureSet) {
       // F-Stop Mode: increase/decrease by fStopStep factor
       float steps = F_STOP_STEPS[fStopStepIndex];
       float factor = pow(2.0f, increase ? steps : -steps);
       exposureTimerValue *= factor;
    } else {
       // Seconds Mode: +/- 0.1s
       if (increase) exposureTimerValue += EXPOSURE_TIMER_STEP;
       else exposureTimerValue -= EXPOSURE_TIMER_STEP;
       
       // Round to avoid floating point drift
       exposureTimerValue = roundToTenth(exposureTimerValue);
    }

    // Bounds check
    if (exposureTimerValue < MIN_EXPOSURE_TIMER_SECONDS) exposureTimerValue = MIN_EXPOSURE_TIMER_SECONDS;
    if (exposureTimerValue > MAX_EXPOSURE_TIMER_SECONDS) exposureTimerValue = MAX_EXPOSURE_TIMER_SECONDS;
    
    setNormalState();
  }
}

// ---------------------------------------------------------------------------
// ROTARY ENCODER 1 INPUT (exposure adjustment)
//
// We read the A/B pins and decode direction using a Gray-code transition table.
// Mechanical encoders bounce, which can produce rapid A/B changes. The table
// assigns partial steps (-1/0/+1) per state transition; a full detent is
// typically 4 transitions. We accumulate those until +/-4 and then emit
// a single logical step. This keeps the UI stable without extra hardware.
//
// Encoder 1 is treated as an alternate input for exposure adjustments and
// does not change the existing btn6/btn7 behavior.
// ---------------------------------------------------------------------------
int8_t readEncoder1Step() {
  static const int8_t transitionTable[16] = {
    0, -1,  1,  0,
    1,  0,  0, -1,
   -1,  0,  0,  1,
    0,  1, -1,  0
  };

  uint8_t a = (digitalRead(ENC1_A_PIN) == LOW) ? 1 : 0;
  uint8_t b = (digitalRead(ENC1_B_PIN) == LOW) ? 1 : 0;
  uint8_t state = (a << 1) | b;

  if (state == encoder1PrevState) return 0;

  // Debounce: ignore rapid transitions within a very short window.
  // This reduces bounce without adding noticeable lag to rotation.
  unsigned long nowMs = millis();
  if (nowMs - encoder1LastTransitionMs < ENCODER1_DEBOUNCE_MS) {
    return 0;
  }
  encoder1LastTransitionMs = nowMs;

  uint8_t index = (encoder1PrevState << 2) | state;
  int8_t fragment = transitionTable[index];
  encoder1PrevState = state;

  if (fragment != 0) {
    encoder1StepAccum += fragment;
    if (encoder1StepAccum >= 4) {
      encoder1StepAccum = 0;
      return ENCODER1_INVERT_DIRECTION ? -1 : 1; // clockwise
    }
    if (encoder1StepAccum <= -4) {
      encoder1StepAccum = 0;
      return ENCODER1_INVERT_DIRECTION ? 1 : -1; // counter-clockwise
    }
  }

  return 0;
}

// Apply one encoder 1 step to the exposure time (same rules as btn6/btn7).
// This does not affect focus or exposure timers while running or paused.
void handleEncoder1ExposureStep(int8_t direction) {
  if (direction == 0) return;

  if (focusTimerRunning || exposureTimerRunning || exposureTimerPaused) return;

  // Acceleration: if detents arrive quickly, apply a larger step.
  // This keeps fine control when turning slowly, but speeds up large changes.
  unsigned long nowMs = millis();
  unsigned long deltaMs = (encoder1LastDetentMs == 0) ? 0 : (nowMs - encoder1LastDetentMs);
  encoder1LastDetentMs = nowMs;

  uint8_t stepMult = 1;
  if (deltaMs > 0 && deltaMs <= ENCODER1_ACCEL_ULTRA_MS) {
    stepMult = ENCODER1_ACCEL_ULTRA_MULT;
  } else if (deltaMs > 0 && deltaMs <= ENCODER1_ACCEL_FAST_MS) {
    stepMult = ENCODER1_ACCEL_FAST_MULT;
  }

  if (stepDisplayActive) {
    stepDisplayActive = false;
  }

  bool increase = (direction > 0);
  if (baseExposureSet) {
    float steps = F_STOP_STEPS[fStopStepIndex];
    float factor = pow(2.0f, increase ? steps : -steps);
    // Apply acceleration by repeating the same f-stop change.
    for (uint8_t i = 0; i < stepMult; i++) {
      exposureTimerValue *= factor;
    }
  } else {
    float delta = EXPOSURE_TIMER_STEP * (float)stepMult;
    if (increase) exposureTimerValue += delta;
    else exposureTimerValue -= delta;
    exposureTimerValue = roundToTenth(exposureTimerValue);
  }

  if (exposureTimerValue < MIN_EXPOSURE_TIMER_SECONDS) exposureTimerValue = MIN_EXPOSURE_TIMER_SECONDS;
  if (exposureTimerValue > MAX_EXPOSURE_TIMER_SECONDS) exposureTimerValue = MAX_EXPOSURE_TIMER_SECONDS;

  setNormalState();
}

#endif

void startExposureTimer() {
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
  hwSetLED(7, 1);  // LED8 ON
  displayTime(exposureTimerCountdown);
}

void pauseExposureTimer() {
  exposureTimerRunning = false;
  exposureTimerPaused = true;
  
  // Account for elapsed time
  unsigned long elapsed = millis() - exposureTimerStartTime;
  exposureBeepAccumulatedMs += elapsed;
  exposureTimerCountdown -= (float)elapsed / 1000.0f;
  if (exposureTimerCountdown < 0.0f) exposureTimerCountdown = 0.0f;
  
  hwSetRelay(false);
  hwSetLED(7, 0);  // LED8 OFF
  displayTime(exposureTimerCountdown);
}

void resumeExposureTimer() {
  exposureTimerRunning = true;
  exposureTimerPaused = false;
  exposureTimerStartTime = millis();
  hwSetRelay(true);
  hwSetLED(7, 1);  // LED8 ON
  displayTime(exposureTimerCountdown);
}

void stopExposureTimer() {
  exposureTimerRunning = false;
  exposureTimerPaused = false;
  exposureTimerCountdown = 0.0f;
  exposureBeepAccumulatedMs = 0;
  exposureLastLongBeepSecond = 0;
  hwSetRelay(false);
  hwSetLED(7, 0);  // LED8 OFF
  setNormalState();
}

void updateExposureTimer() {
  if (exposureTimerRunning) {
    unsigned long elapsed = millis() - exposureTimerStartTime;
    float remaining = exposureTimerCountdown - (float)elapsed / 1000.0f;
    if (remaining <= 0.0f) {
      stopExposureTimer();
    } else {
      displayTime(remaining);
    }
  }
}

void loop() {
  if (ioDiagModeActive) {
    updateIODiagnosticsMode();
    delay(10);
    return;
  }

  // Read button state
  uint8_t buttons = hwReadButton();
  
  // Check for continuous press detection (300 ms threshold)
  bool isContinuousPress = hwCheckContinuousPress(buttons, CONTINUOUS_PRESS_THRESHOLD);

  // Rotary encoder 1: exposure adjustment (replaces btn6/btn7 functionality)
  int8_t encoder1Step = readEncoder1Step();
  handleEncoder1ExposureStep(encoder1Step);


  // ============================================================================
  // BUTTON HANDLING SECTION
  // ============================================================================
  
  switch (buttons) {
    
    // ===== BTN1: Cancel all timers and return to normal state =====
    case btn1:
      if (!btn1PressHandled) {
        btn1PressHandled = true;
        bool anyCancelled = false;
        
        if (focusTimerRunning || focusTimerElapsed > 0) {
          focusTimerClear();
          anyCancelled = true;
        }
        if (exposureTimerRunning || exposureTimerPaused) {
          stopExposureTimer();
          anyCancelled = true;
        }
        
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
      if (exposureTimerRunning) break; 
      
      if (isContinuousPress) {
        if (!focusTimerRunning) {
          if (exposureTimerPaused) stopExposureTimer();
          focusTimerStart();
        }
        focusTimerUpdate();
      } else if (!btn2PressHandled) {
        btn2PressHandled = true;
        if (focusTimerRunning || focusTimerElapsed > 0) {
          focusTimerClear();
        } else {
          // If exposure timer logic was active, clear it first
          if (exposureTimerRunning || exposureTimerPaused) stopExposureTimer();
          
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
      if (focusTimerRunning || exposureTimerRunning) break;
      
      if (stepDisplayActive) {
         stepDisplayActive = false;
         setNormalState();
      }

      if (!exposureTimerRunning && !exposureTimerPaused) {
        handleExposureChange(false, isContinuousPress, btn6PressHandled);
      }
      break;
    
    // ===== BTN7: Increase exposureTimer value (with auto-repeat) =====
    case btn7:
      if (focusTimerRunning || exposureTimerRunning) break;
      
      if (stepDisplayActive) {
         stepDisplayActive = false;
         setNormalState();
      }

      if (!exposureTimerRunning && !exposureTimerPaused) {
        handleExposureChange(true, isContinuousPress, btn7PressHandled);
      }
      break;
    
    // ===== BTN8: Start/Pause/Resume exposureTimer countdown =====
    case btn8:
      if (focusTimerRunning) break;
      
      if (stepDisplayActive) stepDisplayActive = false;

      if (!btn8PressHandled) {
        btn8PressHandled = true;
        if (!exposureTimerRunning && !exposureTimerPaused) {
          startExposureTimer();
        } else if (exposureTimerRunning) {
          pauseExposureTimer();
        } else if (exposureTimerPaused) {
          resumeExposureTimer();
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
        hwSetLED(0, 0);
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
  updateExposureTimer();

  // Update buzzer patterns
  focusTimerBuzzerUpdate();
  exposureTimerBuzzerUpdate();
  
  // ============================================================================
  // STATE TRACKING
  // ============================================================================
  
  // Track last button for release detection
  lastButton = buttons;
  
  delay(10); // Small delay to prevent overwhelming the loop
}
