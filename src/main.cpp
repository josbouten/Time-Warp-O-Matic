/* Time-Warp-O-Matic
   Copyright: Jos Bouten, Dec 2020 and beyond

 This code is based on the V1.0 firmware for the Time Manipulator guitar pedal
 more info at www.electrosmash.com but has undergone quite a few alterations.

 */

 // Uncomment the following line to prime the eeprom (you do not need to erase it!)
// #define PRIME_THE_EEPROM


/* Then upload and run the program once.
   Then comment the line again, compile and upload. The program is ready for use
   now with some values in the eeprom.

    changelog
    ---------
    20 Jan 2026, v0.5
    - added Eeprom-O-Matic, a new eeprom class for storing and reading settings from and to the arduino's eeprom.
    - moved all constants to config.h
    - because Flash room is tight not all strings were banned to program space.
    - the code is too big to fit into an arduino nano IF all the debug prints are active.
      Remedy, if you need to define the DEBUG flag then only uncomment selected debbug_print statements.
      Also, compiled on OSX 12.7.6 the executable turns out always to be larger than when compiled on Linux.
    - replaced stentTimer by MillisDelay which uses less program space.

    June 2024, v0.4
    - added better support for use of the eeprom
    - added a settings struct to make it easy to write them to the eeprom
    - simplified the debug_print so that they do not need LibPrintf (which would cost
      too much memory)

    May/June 2024, v0.3
    - changed CV1 input to clock input
    - added automatic detection of external clock signal
    - added symbolic table to choose delay time when external clock is present

    20. April 2021, v0.2
    - writing settings to eeprom will now happen 10 seconds after the rotary encoder is moved last
      in stead of with every step of the rotary encoder.

    29. Dec 2020, v0.1
    - adapted to usage of module from aliexpress called:
      'PT2399 Microphone Reverb Plate Reverberation Board No Preamplifier Function Module'
    - added an monochrome OLED 32x128 display
    - implemented interrupt service routines for the rotary encoder
    - added a 2nd Short Delay effect (for hardware test purposes only)
    - added a 2nd Echo effect which includes the middle tap signal into the output directly as well
    - added a 3rd Echo effect which adds the input signal to the 2nd tap as well causing a flanging
      effect to occur
    - added a decelerator effect which slows the signal to a halt when a connected foot pedal is pressed down.
    - removed the accelerator effect.
    - added a WOW_is_not_flutter kinda effect.
    - added a 'No Dry Signal to summing mixer' option via a double push of the rotary encoder push button
    - added preset / storage of delay time for each effect in EEPROM
    - repaired the chorus code
    - renamed to Time-Warp-O-Matic

    * Usage:
    * There are 4 modes between which you toggle using the rotary encoder's push button.
    * A: FX selection mode: choose the effect using the rotary encoder.
    * B: Time setting mode: choose the delay time using the rotary encoder.
    * 1: Wet signal only mode: only the wet signal (no direct dry signal) is send to the output
    * 2: Wet and Dry signal mix: the wet signal from the effect units and the dry signal are summed and connected to the output
    * Switch between mode A and B by double pressing the rotary encoder's push button.
    * Switch between mode 1 and 2 by single pressing the rotary encoder's push button.
    *
    * The bypass detect pin is not connected in my eurorack hardware version,
    * but its functionality is fully implemented, so you can add
    * a switch from GND to the BYPASS_DETECT pin if you like.
    *
    * Note on testing the hardware:
    * Bridge the jumper pins on the PCB to get into debug mode. This will:
    * 1: show relevant data on the display
    * 2: allow you to check the attached tap button is functioning
    * 3: allow you to use the rotary encoder to set an FX
    * 4: allow you to use the rotary encoder to set a delay time
    * 5: the delay times will change continually in a slow pace which should be audible.
    *
    *
    * Note: the LED connected to pin 13 on the nano toggles whenever data is written to
    * the internal EEPROM and will blink fast if there is an external clock signal connected.

 */

#include "config.h"
#include "main.h"
#include "Debug.hpp"

#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Rotary.h>
#include "PinChangeInterrupt.h"
#ifdef SCREENSAVER
    #include "MillisDelay.hpp"
    MillisDelay screensaverDelay1;
    MillisDelay screensaverDelay2;
#endif

// // // // // // // // // // // // // //
// See config.h for various settings!  //
// // // // // // // // // // // // // //

// Use this and the jumper on the pcb/vero board to debug the hardware.
// The software will produce some helpful info on the serial port.

#include <OneButton.h>

const char  e0[] PROGMEM = "Deceleratr";
const char  e1[] PROGMEM = "Shrt dly";  // 1 delay line
const char  e2[] PROGMEM = "Lng Delay"; // 2 delay lines in series
const char  e3[] PROGMEM = "Echo";
const char  e4[] PROGMEM = "Echo+";
const char  e5[] PROGMEM = "Echo++";
const char  e6[] PROGMEM = "Chorus";
const char  e7[] PROGMEM = "Chorus+";
const char  e8[] PROGMEM = "Reverb";
const char  e9[] PROGMEM = "WowNotFlut";
const char e10[] PROGMEM = "Telegraph";
const char e11[] PROGMEM = "TeleVerb";
const char e12[] PROGMEM = "Psycho";

const char *const effectName[] PROGMEM = { e0, e1, e2, e3, e4, e5, e6, e7, e8, e9, e10, e11, e12 };

char effectNameStr[12];  // 11 chars + \0 Name of effect shown on the display.

#include "Eeprom.hpp"

Eeprom eeprom;

// Decelerator and Tape Wow constants.
const byte DECELERATOR_COUNTER_MIN = 20;
const byte DECELERATOR_DELAYTIME_MAX = 120;
const byte DECELERATOR_UPDATE_TIME_MAX = 100;
const byte DECELERATOR_UPDATE_TIME_MIN = 10;
const byte wowNotFlutterCounterMin = 20;
const byte wowNotFlutterDelayTimeMax = 60;
const byte WOW_NOT_FLUTTER_TIME_MAX = 60;
byte wowNotFlutterCounter = wowNotFlutterCounterMin;
byte deceleratorCounter = 0;
bool isDeceleratorOnlyOnce = true;

// Setup a Rotary Encoder.
const byte PIN_A = 3; // D3: the first hardware interrupt pin.
const byte PIN_B = 2; // D2: the second hardware interrupt pin.
// Swap the pins if the delay factor decrements if you turn the master control to the right!

// Rotary encoder is wired with the common to ground and the two
// outputs to pins 2 and 3.
Rotary rotary = Rotary(PIN_A, PIN_B);

// Rotary Encoder button.
const byte ENC_PUSH = A1;

// All interrupt vars are volatile in order to force the C++ optimiser to leave them alone.
//volatile byte settings.delayTime[NR_OF_EFFECTS]; // This variable stores the current value of the encoder position for each effect. Change to int or uin16_t instead of byte if you want to record a larger range than 0-255.
volatile byte delayTimeMax[NR_OF_EFFECTS];
volatile byte oldDelayTime[NR_OF_EFFECTS];// Stores the last encoder position value so we can compare to the current reading and see if it has changed (so we know when to print to the serial monitor).
volatile byte coarseDelayTime;
volatile byte reading = 0;   // Somewhere to store the direct values we read from our interrupt pins before checking to see if we have moved a whole detent.

int8_t oldEffect = -1;

byte countDirection = RIGHT;
volatile bool inScreenSaverMode = OFF;
// Create an encoder button.
OneButton button;
// If inSelectMode is true, we can select the effect.
// If select mode is false, we can set a delay time or chorus speed etc.
volatile bool inSelectMode = true;

// Switch C will add some dry signal to the output mix when closed.
volatile bool old_isWetAndDrySelected = false;

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
const uint8_t OLED_RESET = -1; // Reset pin # (or -1 if sharing Arduino reset pin)
// Display is connected to A4 = SDA and A5 = SCL
Adafruit_SSD1306 display(128, 32, &Wire, OLED_RESET);

#ifdef BYPASS_MODE
  #define MAX_MODE_NAME_LEN 12
#endif

unsigned long delayCurrentMillis, delayPreviousMillis = 0;
int directionUp = 1;
unsigned long currentMillis, previousMillis = 0;
unsigned long startTime;
unsigned long endTime;
unsigned long duration = 200;
bool loopb = false;
bool onlyOnce; // This can be used to force the effect name to be shown at startup.

// Effect initialisations.
byte psychoCounter = 0;
byte chorusCounter = 130;

bool writeToEeprom = false;
unsigned long writeTimer = millis();

volatile unsigned long oldTime;
volatile unsigned long thisTime, sumTime, cycleTime;
volatile byte irqCounter = 0;

#define T static_cast<float>(2.0/3.0)
#define S static_cast<float>(3.0/2.0)
//                                          0      1         2       3         4       5      6         7
float baseFactor[NR_OF_MULT_FACTORS] = { T/32.0, 1.0/32.0, T/16.0, 1.0/16.0,   T/8.0, S/16.0, 1.0/8.0, T/4.0,
                                         S/8.0,  1.0/4.0,   T/2.0,   S/4.0,  1.0/2.0,    T,     S/2.0, 1.0 };

const char  s0[] PROGMEM = "1/48";
const char  s1[] PROGMEM = "1/32";
const char  s2[] PROGMEM = "1/24";
const char  s3[] PROGMEM = "1/16";
const char  s4[] PROGMEM = "1/12";
const char  s5[] PROGMEM = "1/16."; // 3/32 dotted sixteenth note
const char  s6[] PROGMEM = "1/8";
const char  s7[] PROGMEM = "1/6";
const char  s8[] PROGMEM = "1/8."; // 3/16 dotted eight note
const char  s9[] PROGMEM = "1/4";
const char s10[] PROGMEM = "1/3";
const char s11[] PROGMEM = "1/4."; // 3/8 = dotted quarter note
const char s12[] PROGMEM = "1/2";
const char s13[] PROGMEM = "2/3";
const char s14[] PROGMEM = "1/2."; // 3/4 = dotted half note
const char s15[] PROGMEM = "1";

const char *const noteDurationStringTable[] PROGMEM = { s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, s11, s12, s13, s14, s15 };

// noteDurationStr to contain time string.
char noteDurationStr[6]; // 5 chars + /0

volatile bool useSymbolicTimeString = false;
volatile bool fineTuneDelayTime = false;


volatile unsigned int ledDelay = LED_DELAY;
volatile unsigned char rotationDirection;

// Prototype to make compilation easier.
void cls(int action);

// Interrupt Service Routines
// ISR that handles a rotation event.
void rotate(void) {
    // countDirection determines whether rotating (counter)clock wise will decrement or increment a
    // counter. It will increment a counter if the counter represents a delay time, if it represents
    // a speed, it will decrement the counter. For each effect the direction is determined when the
    // effect is chosen.
    rotationDirection = rotary.process();
    if (rotationDirection == DIR_CCW) {
        if (inSelectMode == true) {
            if (settings.effect > 0) {
                settings.effect--; // Choose the previous effect.
            } else {
                settings.effect = NR_OF_EFFECTS - 1; // Wrap around.
            }
        } else {
            if (settings.baseFactorIndex[settings.effect] > 0) {
                settings.baseFactorIndex[settings.effect] -= countDirection; // Increment or decrement the effect's speed or delay parameter.
            } else {
                settings.baseFactorIndex[settings.effect] = 0; // Do now Wrap around.
            }
            if (!fineTuneDelayTime) {
                if (useSymbolicTimeString && !isChorusOrReverb) {
                    settings.delayTime[settings.effect] = byte(236.88 * pow(0.9978, baseFactor[settings.baseFactorIndex[settings.effect]] * cycleTime / DIV_FACTOR));
                    debug_print2(F("\npwm value: "), settings.delayTime[settings.effect]);
                    // Copy this delay time just in case we loose the clock and switch to numeric time display.
                    coarseDelayTime = settings.delayTime[settings.effect];
                } else {
                    settings.delayTime[settings.effect] -= countDirection;
                }
            } else {
                // We are fine tuning.
                settings.delayTime[settings.effect] -= countDirection;
                debug_print4(F("\nfine tuning: pwm value: "), settings.delayTime[settings.effect], F(" for effect: "), settings.effect);
            }
            if (settings.delayTime[settings.effect] < DELAY_TIME_MIN) {
                settings.delayTime[settings.effect] = DELAY_TIME_MIN;
            }
        }
    }
    if (rotationDirection == DIR_CW) {
        if (inSelectMode == true) {
            settings.effect++; // Choose the next effect;
            if (settings.effect > NR_OF_EFFECTS - 1) {
                settings.effect = 0; // Wrap around.
            }
        } else {
            settings.baseFactorIndex[settings.effect] += countDirection; // Increment or decrement the effect's speed or delay parameter.
            if (settings.baseFactorIndex[settings.effect] > NR_OF_MULT_FACTORS - 1) {
                settings.baseFactorIndex[settings.effect] = NR_OF_MULT_FACTORS - 1;
            }
            if (!fineTuneDelayTime) {
                if (useSymbolicTimeString && !isChorusOrReverb) {
                    settings.delayTime[settings.effect] = byte(236.88 * pow(0.9978, baseFactor[settings.baseFactorIndex[settings.effect]] * cycleTime / DIV_FACTOR));
                    //debug_print2(F("\npwm value: "), settings.delayTime[settings.effect]);
                    // Copy this delay time just in case we loose the clock and switch to numeric time display.
                    coarseDelayTime = settings.delayTime[settings.effect];
                } else {
                    settings.delayTime[settings.effect] += countDirection;
                }
            } else {
                // We are fine tuning.
                settings.delayTime[settings.effect] += countDirection;
                //debug_print4("\nfine tuning: pwm value: ", settings.delayTime[settings.effect], " for effect: ", settings.effect);
            }
            if (settings.delayTime[settings.effect] > delayTimeMax[settings.effect]) {
                settings.delayTime[settings.effect] = delayTimeMax[settings.effect];
            }
        }
    }
    // Start new countdown.
    #ifdef SCREENSAVER
        if (inScreenSaverMode == ON) {
            inScreenSaverMode = OFF;
        }
        screensaverDelay1.start();
        screensaverDelay2.start();
    #endif
}

// The Interrupt Service Routine for ENC_PUSH Change Interrupt 1
// This routine will only be called on any signal change on ENC_PUSH (H.L. A1): exactly where we need to check.
void buttonTick() {
    // keep watching the push button:
    noInterrupts(); // Stop interrupts happening before we read pin values.
    button.tick();  // Call tick() to check the state.
    interrupts();
}

void extClock() {
    debug_print_ncr(F("C"));
    irqCounter++;
    thisTime = micros();
    sumTime += thisTime - oldTime;
    oldTime = thisTime;
    // We use the mean of several inter interrupt times as the cycle time.
    if (irqCounter > NR_OF_CYCLES) {
        cycleTime = sumTime / irqCounter;
        if (! fineTuneDelayTime) {
            byte tmp = byte(236.88 * pow(0.9978, baseFactor[settings.baseFactorIndex[settings.effect]] * cycleTime / 1000));
            if (tmp > delayTimeMax[settings.effect]) {
                tmp = delayTimeMax[settings.effect];
            }
            if (tmp < DELAY_TIME_MIN) {
                tmp = DELAY_TIME_MIN;
            }
            settings.delayTime[settings.effect] = tmp;
        }
        debug_print4("\t", settings.effect, "\t", settings.delayTime[settings.effect]);
        // Now the cycleTime is known, we can set a delay time by choosing
        // between note intervals as defined in the baseFactor vector.
        useSymbolicTimeString = true;
        ledDelay = LED_DELAY / 10;
        irqCounter = 0;
        sumTime = 0;
    }
    button.tick();
}

void displayText(String dt_line_of_text, int dt_fieldLength, int dt_row, int dt_column, byte dt_clear_mode = CLEAR_LOCAL, int dt_fontSize = 2) {
    // fieldLength: length of field to display line_of_text in.
    // This part will be erased when clear_local is set to true.
    if (dt_fieldLength == 0) {
        dt_fieldLength = dt_line_of_text.length();
    }
    display.setTextColor(SSD1306_WHITE); // Draw white text
    display.cp437(true);                 // Use full 256 char 'Code Page 437' font
    switch(dt_clear_mode){
        case CLEAR_LINE:
            if (dt_fieldLength * 7 * dt_fontSize < display.width()) {
                display.fillRect(8 * dt_column, 8 * dt_row, display.width() - dt_fieldLength * 7 * dt_fontSize, 8 * dt_fontSize, SSD1306_BLACK);
            } else {
                display.fillRect(8 * dt_column, 8 * dt_row, display.width(), 8 * dt_fontSize, SSD1306_BLACK);
            }
            break;
        case CLEAR_LOCAL:
            display.fillRect(8 * dt_column, 8 * dt_row, dt_fieldLength * 7 * dt_fontSize, 8 * dt_fontSize, SSD1306_BLACK);
            break;
        case CLEAR_NOT:
            break;
        // default:
        //     debug_print(F("Unknown mode"));
    }
    display.setCursor(dt_column * 8, dt_row * 8);
    display.setTextColor(WHITE, BLACK);
    display.print(dt_line_of_text);
    // Now show the text on the display.
    display.display();
}

void setDelayTime() {
    // Toggle from selecting an effect type to setting a time constant for the delays used.
    // Called when clicking the rotary encoder's button.
    debug_print(F("single click"));
    inSelectMode = !inSelectMode;
    // Clear line 1 and 2.
    displayText("", 0, 0, 0, CLEAR_LINE, 2);
    displayText("", 0, 1, 0, CLEAR_LINE, 2);
    display.clearDisplay();
    // Force entering effect switch in loop() and update W+D;
    loopb = true;
    old_isWetAndDrySelected = !settings.isWetAndDrySelected;
    inScreenSaverMode = OFF;
}

void finetuneDelayTime() {
    // Called when double clicking the rotary encoder's button.
    debug_print(F("double click"));
    fineTuneDelayTime = !fineTuneDelayTime;
}

void wetDryToggle() {
    // Toggle between adding the dry signal to the output and not sending the dry input signal to the output mixer.
    // Called when long clicking the rotary encoder's button.
    debug_print(F("encoder long click"));

    switch(settings.effect) {
        // Wet and Dry signal.
        case SHORT_DELAY:
        case DELAY:
        case ECHO1:
        case ECHO2:
        case ECHO3:
        case REVERB:
        case PSYCHO:
            settings.isWetAndDrySelected = !settings.isWetAndDrySelected;
            digitalWrite(SWC, settings.isWetAndDrySelected);
            break;
        // Wet only signal.
        case DECELERATOR:
        case CHORUS:
        case FAST_CHORUS:
        case WOW_NOT_FLUTTER:
        case TELEGRAPH:
        case TELEVERB:
        break;
        // default:
        //     debug_print2(F("HWGAP, effect value: "), settings.effect);
    }
    inScreenSaverMode = OFF;
}

void setSwitches(byte ss_swa, byte ss_swb, byte ss_swc, byte ss_swd) {
    // Set analog switches of CD4066.
    digitalWrite(SWA, ss_swa);
    digitalWrite(SWB, ss_swb);
    digitalWrite(SWC, ss_swc);
    digitalWrite(SWD, ss_swd);
}

void printSettings() {
    for (uint8_t effectNr = 0; effectNr < NR_OF_EFFECTS; effectNr++) {
        strcpy_P(noteDurationStr, reinterpret_cast<char *>(pgm_read_ptr( &(noteDurationStringTable[settings.baseFactorIndex[effectNr]]))));  // Necessary casts and dereferencing, just copy.
        strcpy_P(effectNameStr, reinterpret_cast<char *>(pgm_read_ptr( &(effectName[effectNr]))));  // Necessary casts and dereferencing, just copy.

        if ( (effectNr == CHORUS) || (effectNr == FAST_CHORUS)) {
            debug_print4(F("fFX["), effectNameStr, F("]: spd: "), settings.delayTime[effectNr]);
        } else {
            debug_print6(F("FX["), effectNameStr, F("]: delTime: "), settings.delayTime[effectNr], F(" baseFactor: "), noteDurationStr);
        }
    }
    debug_print2(F("isWetAndDrySelected: "), settings.isWetAndDrySelected == true ? F("true"): F("false"));
    debug_print2(F("Current FX: "), settings.effect);
}

void updateEepromTimer(void) {
    debug_print("-");
    writeToEeprom = true;
    writeTimer = millis() + DELAY_TIME_BEFORE_WRITING_TO_EEPROM_IN_MS;
}

void writeSettingsToEeprom(void) {
    if ((writeToEeprom == true) && (millis() > writeTimer)) {
        writeToEeprom = false;
        #ifdef WRITE_TO_EEPROM
            debug_print3("\nWriting (", sizeof(SettingsObjType), " bytes) to EEPROM.");
            eeprom.write(settings);
            printSettings();
            digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
            delay(100);
            digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
        #else
            debug_print("\nNot writing settings to EEPROM.");
        #endif
    }
}

void setDelays(byte sd_delay_time1, byte sd_delay_time2) {
    // Set PWM on/off ratio for TL072B-1/2.
    // PWM values must range between 0 and 255
    analogWrite(DELAY1, sd_delay_time1);
    analogWrite(DELAY2, sd_delay_time2);
}

void showFillCircle(byte sfc_mode) {
    for (uint8_t i = 0; i < DISPLAY_WIDTH / 2; i += 3) {
        // The INVERSE color is used so circles alternate white/black
        display.fillCircle(display.width() / 2, display.height() / 2, i, sfc_mode);
        // Update screen with each newly-drawn circle
        display.display();
        delay(1);
    }
}

void showSplashScreen(void) {
    display.clearDisplay();
    display.setTextSize(2); // Normal 2:1 pixel scale
    display.clearDisplay();
    displayText(F("Time"), 0, 1, 4);
    display.display();
    delay(600);
    display.clearDisplay();
    displayText(F("Warp"), 0, 1, 4);
    display.display();
    delay(600);
    display.clearDisplay();
    displayText(F("-O-"), 0, 1, 5);
    display.display();
    delay(600);
    showFillCircle(SSD1306_INVERSE);
    showFillCircle(SSD1306_BLACK);
    delay(200);
    display.clearDisplay();
    displayText(F("Matic"), 0, 1, 3);
    display.display();
    delay(600);
    displayText(PGM_VERSION, 7, 1, 3, CLEAR_LOCAL, 2);
    delay(1200);
    display.clearDisplay();
    display.display();
}

void setupDisplay() {
    // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
    while (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3C for 128x32
        // Trying to allocate WIDTH * ((HEIGHT + 7) / 8) bytes
        // 128 * ((32 + 7) / 8) = 624
        debug_print(F("SSD1306 allocation failed."));
        delay(1000);
    }
    debug_print(F("SSD1306 allocation succeeded!"));
    display.clearDisplay();
    display.setTextSize(2);  // Normal 2:1 pixel scale
}

#ifdef SCREENSAVER
  void cls(int action){
    switch(action) {
      case CLS_TIMER_ACTION:
        display.clearDisplay();
        display.display();
        inScreenSaverMode = ON;
        break;
      case SCREENSAVER_TIMER_ACTION:
        if (inScreenSaverMode == ON) {
            showFillCircle(SSD1306_INVERSE);
            showFillCircle(SSD1306_BLACK);
        }
    }
    // Serial.print(F("Timer call -> Action: "));
    // Serial.print(action);
    // Serial.print(F(", Current Time: "));
    // Serial.println(millis());
  }

    void screensaverTick() {
        if (screensaverDelay1.justFinished()) {
            screensaverDelay1.start();
            Serial.print("BLA1\n");
            cls(CLS_TIMER_ACTION);
        }
        if (screensaverDelay2.justFinished()) {
            screensaverDelay2.start();
            Serial.print("BLA2\n");
            cls(SCREENSAVER_TIMER_ACTION);
        }
    }
#endif

void setup() {
    // Set serial device, only for debug purposes
    Serial.begin(230400);
    debug_print(F("Started setup."));

      // EEPROM
    eeprom = Eeprom(EEPROM.length());

    #ifdef PRIME_THE_EEPROM
        settings.effect = INITIAL_EFFECT;
        settings.isWetAndDrySelected = true;
        for (uint8_t effect = 0; effect < NR_OF_EFFECTS; effect++) {
            settings.baseFactorIndex[effect] = INITIAL_BASE_FACTOR;
            settings.delayTime[effect] = INITIAL_DELAY_TIME;
        }
        settings.effect = INITIAL_EFFECT;
        settings.isWetAndDrySelected = true;
        eeprom.prime(settings);
    #else
        eeprom.read(&settings);

        // If the eeprom was not empty, its content was read by its constructor.
        // Show the settings.
        printSettings();

        setupDisplay();
        // Show Splash screen.
        showSplashScreen();

        // Set the pins
        pinMode(DELAY1,   OUTPUT);
        pinMode(DELAY2,   OUTPUT);
        pinMode(SWA,      OUTPUT);
        pinMode(SWB,      OUTPUT);
        pinMode(SWC,      OUTPUT);
        pinMode(SWD,      OUTPUT);
        pinMode(ENC_PUSH, INPUT_PULLUP); // switch of rotary encoder.
        // The bypass detect pin is not connected in the eurorack version,
        // but its functionality is fully implemented, so you can add
        // a switch from GND to the BYPASS_DETECT pin if you like.
        pinMode(BYPASS_DETECT, INPUT_PULLUP);
        pinMode(PEDAL_SWITCH,  INPUT_PULLUP);
        pinMode(CV1,           INPUT);

        // Finally get the state of the isWetAndDrySelected variable.
        old_isWetAndDrySelected = settings.isWetAndDrySelected;

        // Initialize the PWM outputs for the current effect.
        setDelays(settings.delayTime[settings.effect], settings.delayTime[settings.effect]);

        // Set a few max values for the counters.
        for (uint8_t i = 0; i < NR_OF_EFFECTS; i++) {
        delayTimeMax[i] = 255;
        }
        delayTimeMax[DECELERATOR] = DECELERATOR_UPDATE_TIME_MAX;
        delayTimeMax[SHORT_DELAY] = 255;
        delayTimeMax[WOW_NOT_FLUTTER] = WOW_NOT_FLUTTER_TIME_MAX;

        // Initialize the CD4066 switches.
        // Allow some signal to flow into the 2nd tap.
        setSwitches(LOW, HIGH, LOW, LOW);

        // Initialize ports for rotary encoder
        pinMode(PIN_A, INPUT_PULLUP); // Set PIN_A as an input, pulled HIGH to the logic voltage (5V or 3.3V for most cases)
        pinMode(PIN_B, INPUT_PULLUP); // Set PIN_B as an input, pulled HIGH to the logic voltage (5V or 3.3V for most cases)

        // Set hardware interrupts for rotary encoder.
        attachInterrupt(0, rotate, CHANGE);
        attachInterrupt(1, rotate, CHANGE);

        // Attach methods to button clicks.
        button = OneButton(ENC_PUSH, true, true);
        debug_print(F("Button config started."));
        button.attachClick(setDelayTime);
        button.attachDoubleClick(finetuneDelayTime);
        button.attachLongPressStart(wetDryToggle);
        debug_print("Button config finished.");

        // Initialize old counter values.
        for (byte i = 0; i < NR_OF_EFFECTS; i++) {
        oldDelayTime[i] = settings.delayTime[i];
        }

        // Init the note duration string. Necessary casts and dereferencing, just copy.
        strcpy_P(noteDurationStr, reinterpret_cast<char *>(pgm_read_ptr( &(noteDurationStringTable[settings.baseFactorIndex[settings.effect]]))));
        debug_print2("Initial note duration string: ", noteDurationStr);

        // // For cycle time calculation.
        oldTime = micros();

        // Attach interrupt service routine to ENC_PUSH and call button.tick in order to get
        // a fast response for 'encoder click' and 'encoder double click' events.
        noInterrupts();
        // Attach ISR routine for Pin Change Interrupt on ENC_PUSH (= A1).
        attachPinChangeInterrupt(digitalPinToPinChangeInterrupt(ENC_PUSH), buttonTick, RISING);
        // Attach ISR routine for Pin Change Interrupt on CV1.
        attachPinChangeInterrupt(digitalPinToPinChangeInterrupt(CV1), extClock, RISING);
        interrupts();

        // Restore switch C so that we hear a mix of the dry and wet signal or only the wet signal..
        digitalWrite(SWC, settings.isWetAndDrySelected);
        // Initialize some vars for main loop.
        onlyOnce = true; // This forces the effect name to be shown at startup.
        oldEffect = settings.effect; // This prevents writing to the eeprom at startup.
        // We allow for some time to get to a stable value for the cycle time. This will prevent the settings to
        // be written to the eeprom although there has not been a change. Note this will only work if the present
        // cycle time is the same as the one when the module was running the last time.
        debug_print(F("Waiting for cycle time estimate to become stable."));
        delay(6000);
        oldDelayTime[settings.effect] = settings.delayTime[settings.effect];

        #ifdef SCREENSAVER
            screensaverDelay1 = MillisDelay(SCREENCLS_DELAY);
            screensaverDelay1.start();
            screensaverDelay2 = MillisDelay(SCREENSAVER_DELAY);
            screensaverDelay2.start();
        #endif
    #endif
    debug_print(F("Setup Finished"));
}

#ifdef ALIVE
void alive() {
    static unsigned long oldLedBlinkTime = millis();
    if ((millis() - oldLedBlinkTime) > ledDelay) {
        digitalWrite(LED_BUILTIN, ! digitalRead(LED_BUILTIN));
        oldLedBlinkTime = millis();
    }
}
#endif

#ifdef PRIME_THE_EEPROM
    void loop() {
        delay(1000);
        debug_print("Recompile and upload with undefined PRIME_THE_EEPROM");
    }
#else
    void loop() {
        static uint8_t delta = 1;
        static float deltaSpeed = 1.0;
        uint8_t td = 0;

        #ifdef BYPASS_MODE
            // Detect if the effect is on or off.
            while((digitalRead(BYPASS_DETECT) == LOW)) {
                // Going into bypass mode. To hear anything the W/D potentiometer should be turned to W.
                setSwitches(LOW, LOW, HIGH, LOW);
                display.setTextSize(1);
                displayText(F("Mode: bypass"), MAX_MODE_NAME_LEN, 0, 0, CLEAR_LINE, 2);
            }
        #endif

        display.setTextSize(2);
        button.tick();
        // If in settings mode, show the parameter's name "speed" or "time" and relative or absolute value.
        if ((inSelectMode == false) && (inScreenSaverMode == OFF)) {
            switch(settings.effect) {
                case(DECELERATOR):
                    displayText(F("Speed:"), 0, 0, 0, CLEAR_LINE, 2);
                    displayText((String) (delayTimeMax[settings.effect] - settings.delayTime[settings.effect]), 0, 0, COUNTER_POSITION + 2, CLEAR_LINE, 2);
                break;
                case(CHORUS):
                case(FAST_CHORUS):
                    displayText(F("Speed:"), 0, 0, 0, CLEAR_LINE, 2);
                    displayText((String) (CHORUS_UPPER_LIMIT - settings.delayTime[settings.effect]), 0, 0, COUNTER_POSITION + 2, CLEAR_LINE, 2);
                break;
                case(REVERB):
                case(WOW_NOT_FLUTTER):
                default:
                    // This will handle reverb and delay effects.
                    displayText(F("Time"), 4, 0, 0, CLEAR_LOCAL);
                    // Show the note duration string, e.g. T/4 or 1/16.
                    if (fineTuneDelayTime) {
                        // Show difference between coarse and fine tuned value.
                        // Erase old value, show new value.
                        displayText(String(settings.delayTime[settings.effect] - coarseDelayTime), 5, 2, NOTES_STR_POSITION - 1, CLEAR_LOCAL);
                    } else {
                        // Erase fine tuning info
                        displayText(String(F("  ")), 3, 2, NOTES_STR_POSITION - 1, CLEAR_LOCAL, 2);
                        if (useSymbolicTimeString) {
                            strcpy_P(noteDurationStr, reinterpret_cast<char *>(pgm_read_ptr( &(noteDurationStringTable[settings.baseFactorIndex[settings.effect]]))));  // Necessary casts and dereferencing, just copy.
                            displayText(String(noteDurationStr), 6, 0, NOTES_STR_POSITION, CLEAR_LOCAL, 2);
                        } else {
                            displayText(String(settings.delayTime[settings.effect]), 0, 0, COUNTER_POSITION + 1, CLEAR_LINE, 2);
                        }
                    }
                    break;
            }
        }

        button.tick();

        // This is the main effect loop. Here we set the analog switches
        // and the delay times.
        // loopb should be set to 'true' for an effect if:
        // the effect depends on the 'PEDAL_SWITCH' button.
        // In the first series of statement we set the switches and delay times.
        // This happens only of some value was changed.
        // In the second series of statements we display the delay or speed value.

        if (settings.effect != oldEffect) {
            isDeceleratorOnlyOnce = true;
        }

        if ((settings.effect != oldEffect) || (settings.delayTime[settings.effect] != oldDelayTime[settings.effect])) {
            updateEepromTimer();
        }

        if ((settings.effect != oldEffect) ||
            (settings.delayTime[settings.effect] != oldDelayTime[settings.effect]) ||
            (loopb == true) || (onlyOnce == true)) {
            // Set default values for countDirection and loopb, these may be changed by certain effect numbers below.
            onlyOnce = false;
            countDirection = RIGHT;
            loopb = false;
            oldEffect = settings.effect;
            oldDelayTime[settings.effect] = settings.delayTime[settings.effect];
            if ((inSelectMode == true) && (inScreenSaverMode == OFF)) {
                // Necessary casts and dereferencing, just copy.
                strcpy_P(effectNameStr, reinterpret_cast<char *>(pgm_read_ptr( &(effectName[settings.effect]))));
                //display.clearDisplay();
                //debug_print(".");
                displayText(effectNameStr, MAX_FX_NAME_LEN, 0, 0, CLEAR_LINE, 2);
            }
            switch(settings.effect) {
                case(DECELERATOR):
                    loopb = true;
                    setSwitches(LOW, LOW, LOW, LOW);
                    if (digitalRead(PEDAL_SWITCH) == HIGH) {
                        isDeceleratorOnlyOnce = true;
                    }
                    if (isDeceleratorOnlyOnce == true) {
                        if (digitalRead(PEDAL_SWITCH) == LOW) {
                            setSwitches(LOW, HIGH, LOW, LOW);
                            isDeceleratorOnlyOnce = false;
                        } else {
                            setSwitches(LOW, LOW, LOW, LOW);
                        }
                        deceleratorCounter = DECELERATOR_DELAYTIME_MAX;
                    }
                    if (settings.delayTime[DECELERATOR] > DECELERATOR_UPDATE_TIME_MAX) {
                        settings.delayTime[DECELERATOR] = DECELERATOR_UPDATE_TIME_MAX;
                    }
                    if (settings.delayTime[DECELERATOR] < DECELERATOR_UPDATE_TIME_MIN) {
                        settings.delayTime[DECELERATOR] = DECELERATOR_UPDATE_TIME_MIN;
                    }
                    // If TAP footswitch is pushed, reduce the delay time (deceleratorCounter) c.q. "slow down" the signal.
                    if ((digitalRead(PEDAL_SWITCH) == LOW) && (deceleratorCounter >= DECELERATOR_COUNTER_MIN)) {
                        // The lines below introduce a delay, so the delay time (deceleratorCounter) is not reduced too fast.
                        delayCurrentMillis = millis();
                        if ((delayCurrentMillis - delayPreviousMillis) >= settings.delayTime[DECELERATOR]) {
                            delayPreviousMillis = delayCurrentMillis;
                            deceleratorCounter--;
                        }
                        if ((deceleratorCounter == DECELERATOR_COUNTER_MIN) && (digitalRead(PEDAL_SWITCH) == LOW)) {
                            setSwitches(LOW, LOW, LOW, LOW);
                        }
                    }
                    // Update the delay time.
                    setDelays(deceleratorCounter, deceleratorCounter);
                break;

                case(SHORT_DELAY):
                    setSwitches(HIGH, LOW, settings.isWetAndDrySelected, LOW);
                    setDelays(settings.delayTime[SHORT_DELAY], settings.delayTime[SHORT_DELAY]);
                break;

                case(DELAY):
                    // Do not include the tap 1 signal directly in the output.
                    setSwitches(LOW, HIGH, settings.isWetAndDrySelected, LOW);
                    setDelays(settings.delayTime[DELAY], settings.delayTime[DELAY]);
                break;

                case(ECHO1):
                    // Feed forward the dry signal to the 2nd tap.
                    setSwitches(LOW, HIGH, settings.isWetAndDrySelected, HIGH);
                    setDelays(settings.delayTime[ECHO1], settings.delayTime[ECHO1]);
                break;

                case(ECHO2):
                    // Do not feed forward the dry signal to the 2nd tap.
                    setSwitches(HIGH, HIGH, settings.isWetAndDrySelected, LOW);
                    setDelays(settings.delayTime[ECHO2], settings.delayTime[ECHO2]);
                break;

                case(ECHO3):
                    // Include the 'middle tap' signal directly in the output as well.
                    setSwitches(HIGH, HIGH, settings.isWetAndDrySelected, HIGH);
                    setDelays(settings.delayTime[ECHO3], settings.delayTime[ECHO3]);
                break;

                case(CHORUS):
                    delta = 2;
                    // fall through into case(FAST_CHORUS)
                case(FAST_CHORUS):
                    delta = 2;
                    loopb = true;
                    setSwitches(HIGH, LOW, !digitalRead(PEDAL_SWITCH), HIGH);
                    if (directionUp == 1) {
                        if (chorusCounter > CHORUS_UPPER_LIMIT) {
                            chorusCounter = CHORUS_UPPER_LIMIT;
                            directionUp = 0;
                        }
                        // Delay created by millis();
                        delayCurrentMillis = millis();
                        if ((delayCurrentMillis - delayPreviousMillis) > (unsigned int) (MIN_TIME + (settings.delayTime[CHORUS] >> 1))) {
                            delayPreviousMillis = delayCurrentMillis;
                            chorusCounter += delta; // If too fast try divider.
                        }
                    } else {
                        if (chorusCounter < CHORUS_LOWER_LIMIT) {
                            chorusCounter = CHORUS_LOWER_LIMIT;
                            directionUp = 1;
                        }
                        // Delay created by millis();
                        delayCurrentMillis = millis();
                        if ((delayCurrentMillis - delayPreviousMillis) > (unsigned int) (MIN_TIME + (settings.delayTime[CHORUS] >> 1))) {
                            delayPreviousMillis = delayCurrentMillis;
                            chorusCounter -= delta; // If too fast try divider.
                        }
                    }
                    setDelays(chorusCounter, 440 - chorusCounter); // ToDo: Why 440 - ???? The delay value is a byte, so 0 <= delayTime <= 255 !
                break;

                case(REVERB):
                    setSwitches(HIGH, LOW, settings.isWetAndDrySelected, HIGH);
                    setDelays(MAX_COUNTER, MAX_COUNTER - (settings.delayTime[REVERB] >> 1)); // One delay is 1/2 the other.
                    countDirection = LEFT;
                break;

                case(WOW_NOT_FLUTTER):
                    loopb = true;
                    setSwitches(HIGH, LOW, LOW, LOW);
                    delta = random(1, 11);
                    deltaSpeed = random(100, 401) / 100.0;
                    wowNotFlutterCounter = wowNotFlutterCounterMin;

                    // counter[WOW_NOT_FLUTTER] determines the speed of changing the delay time.
                    // delta determines the step size of the change in delay time.
                    delayCurrentMillis = millis();
                    if (delayCurrentMillis - delayPreviousMillis >= deltaSpeed * settings.delayTime[WOW_NOT_FLUTTER]) {
                        delayPreviousMillis = delayCurrentMillis;
                        wowNotFlutterCounter += delta;
                        if (wowNotFlutterCounter < wowNotFlutterCounterMin) {
                        // Choose a positive value for delta.
                        delta = random(0, 5);
                        deltaSpeed = (settings.delayTime[WOW_NOT_FLUTTER] / wowNotFlutterDelayTimeMax) * random(100, 401) / 100.0;
                        } else {
                        if (wowNotFlutterCounter > wowNotFlutterDelayTimeMax) {
                            // Choose a negative value for delta.
                            delta = -random(0, 5);
                            deltaSpeed = (settings.delayTime[WOW_NOT_FLUTTER] / wowNotFlutterDelayTimeMax) * random(100, 401) / 100.0;
                        }
                        }
                    }
                    // Update the delay time.
                    setDelays(wowNotFlutterCounter * 2, wowNotFlutterCounter * 2);
                    countDirection = LEFT;
                break;

                case(TELEGRAPH):
                    loopb = true;
                    setSwitches(!digitalRead(PEDAL_SWITCH), LOW, HIGH, LOW);
                    countDirection = LEFT;
                break;

                case(TELEVERB): // Switch reverbed signal on using the tap cv input.
                    loopb = true;
                    td = !digitalRead(PEDAL_SWITCH);
                    setSwitches(td, LOW, td, td);
                    setDelays(220, 220 - (settings.delayTime[REVERB] >> 1));
                    countDirection = LEFT;
                break;

                case(PSYCHO):
                    loopb = true;
                    setSwitches(HIGH, HIGH, settings.isWetAndDrySelected, HIGH);
                    if (directionUp == 1) {
                        psychoCounter++; // If too fast try divider.
                        if (psychoCounter > 220) {
                            psychoCounter = 220;
                            directionUp = 0;
                        }
                        delay(settings.delayTime[PSYCHO] >> 1);
                    } else {
                        psychoCounter--; // If too fast try divider.
                        if (psychoCounter < 50) {
                            psychoCounter = 20;
                            directionUp = 1;
                        }
                        delay(settings.delayTime[PSYCHO] >> 1);
                    }
                    if ((psychoCounter >> 1) > 50) {
                        analogWrite(DELAY1, psychoCounter >> 1);
                    } else {
                        analogWrite(DELAY1, 50);
                    }
                    analogWrite(DELAY2, 270 - psychoCounter);
                    countDirection = LEFT;
                break;
                // default:
                //     debug_print("2: Unknown effect value");
                //     settings.effect = INITIAL_EFFECT;
                // break;
            }
        }

        button.tick();
        // Update wet/dry status in display only when it has changed.
        if ((old_isWetAndDrySelected != settings.isWetAndDrySelected) || (digitalRead(PEDAL_SWITCH) == HIGH)) {
            old_isWetAndDrySelected = settings.isWetAndDrySelected;
            display.setTextSize(1);
            if (inScreenSaverMode == OFF) { // Only show the status when the screen saver is off.
                switch(settings.effect) {
                case SHORT_DELAY:
                case DELAY:
                case ECHO1:
                case ECHO2:
                case ECHO3:
                case REVERB:
                case CHORUS:
                case FAST_CHORUS:
                case PSYCHO:
                case TELEGRAPH:
                    if (settings.isWetAndDrySelected == false) {// Only wet signal on output.
                        displayText((String) WT, 4, 3, MAX_FX_NAME_LEN + 1, CLEAR_LOCAL, 1);
                    } else { // Wet and dry signal on output.
                        displayText((String) WD, 4, 3, MAX_FX_NAME_LEN + 1, CLEAR_LOCAL, 1);
                    }
                    break;
                case DECELERATOR:
                case WOW_NOT_FLUTTER:
                    displayText((String) WT, 4, 3, MAX_FX_NAME_LEN + 1, CLEAR_LOCAL, 1);
                    break;
                case TELEVERB:
                    if (digitalRead(PEDAL_SWITCH) == HIGH) { // Wet+Dry when switch closes contacts.
                        displayText((String) WD, 4, 3, MAX_FX_NAME_LEN + 1, CLEAR_LOCAL, 1);
                    } else {
                        displayText((String) WT, 4, 3, MAX_FX_NAME_LEN + 1, CLEAR_LOCAL, 1);
                    }
                    break;
                }
                display.setTextSize(2);
            }
        }
        button.tick();
        #ifdef SCREENSAVER
            screensaverTick();
        #endif
        writeSettingsToEeprom();
        #ifdef ALIVE
            alive();
        #endif
        // thisTime is set when receiving an external clock pulse.
        // So if that was too long ago, we conclude that the clock pulse is not there
        // anymore and switch from symbolic delay times to integer values.
        if (useSymbolicTimeString & ((micros() - thisTime) > EXT_CLOCK_TIMEOUT)) {
            // clear the display of symbols if any are shown and show the discrete delay value.
            if (!inSelectMode) {
                displayText(String(F("   ")), 5, 0, NOTES_STR_POSITION - 2, CLEAR_LOCAL, 2);
                displayText(String(settings.delayTime[settings.effect]), 0, 0, COUNTER_POSITION + 1, CLEAR_LINE, 2);
            }
            debug_print(F("#"));
            useSymbolicTimeString = false;
            ledDelay = LED_DELAY;
        }
    }
#endif