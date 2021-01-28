/* Time-Warp-O-Matic by Jos Bouten, Dec 2020, Jan 2021

 This code is based on the V1.0 firmware for the Time Manipulator guitar pedal
 more info at www.electrosmash.com but has undergone quite a few alterations.
 
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
 * Testing the indivual PT2399 boards can be done using the SHORT_DELAY1 and SHORT_DELAY2
 * effect. SHORT_DELAY2 will only exist if you set the DEBUG flag (don't forget to compile and upload the binary 
 * code to the arduino). Test both the circuits checking their sound with and without the dry input signal 
 * (using the double click function of the rotary encoder). If all is well, then comment out the DEBUG 
 * flag, recompile and upload the binary code to the arduino.
 * 
 * Note: the LED connected to pin 13 on the nano toggles whenever data is written to
 * the internal EEPROM.
 
 */
 
#include <EEPROM.h> 
#include <Rotary.h>

#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <StensTimerJSB.h>

//#define DEBUG 
// Use this and the jumper on the pcb/vero board to debug the hardware.
// The software will produce some helpful info on the serial port.

#include <OneButton.h>

const String pgm_version = "v0.1";

#define ON true
#define OFF false

// Effects definitions.
#ifndef DEBUG
#define DECELERATOR      0 
#define SHORT_DELAY1     1
#define DELAY            2
#define ECHO1            3 
#define ECHO2            4 
#define ECHO3            5
#define REVERB           6 
#define CHORUS           7 
#define FAST_CHORUS      8 
#define WOW_NOT_FLUTTER  9
#define TELEGRAPH       10
#define TELEVERB        11
#define PSYCHO          12
#define NR_OF_EFFECTS   13
#else
// When debugging the hardware using the DEBUG flag
// SHORT_DELAY2 is added to test the 2nd PT chip board.
#define DECELERATOR      0 
#define SHORT_DELAY1     1 
#define SHORT_DELAY2     2 
#define DELAY            3 
#define ECHO1            4 
#define ECHO2            5 
#define ECHO3            6
#define CHORUS           7 
#define FAST_CHORUS      8 
#define REVERB           9 
#define WOW_NOT_FLUTTER 10
#define TELEGRAPH       11 
#define TELEVERB        12 
#define PSYCHO          13
#define NR_OF_EFFECTS   14
#endif

// EEPROM message memory locations.
#define EFFECT 0
#define COUNTER 1 // 1 ... NR_OF_EFFECTS
#define NO_DRY_SIGNAL (NR_OF_EFFECTS + 1)

// Chorus time constants.
#define MIN_TIME 80
#define CHORUS_LOWER_LIMIT 185
#define CHORUS_UPPER_LIMIT 250
     
#define MAX_FX_NAME_LEN 12

// Decelerator and Tape Wow constants.
const byte DECELERATOR_counter_min = 20;
const byte DECELERATOR_counter_max = 120;
const byte DECELERATOR_counter_start = 90;
const byte DECELERATOR_UPDATE_TIME_MAX = 100;       
const byte DECELERATOR_UPDATE_TIME_MIN = 10;
const byte WOW_NOT_FLUTTER_counter_min = 20;
const byte WOW_NOT_FLUTTER_counter_max = 60;
const byte WOW_NOT_FLUTTER_TIME_MAX = WOW_NOT_FLUTTER_counter_max;
byte WOW_NOT_FLUTTER_counter = WOW_NOT_FLUTTER_counter_min;
byte DECELERATOR_counter = 0;
bool DECELERATOR_only_once = true;
#define WOW_NOT_FLUTTER_DELTA 10

// Setup a Rotary Encoder.
static byte pinA = 3; // The first hardware interrupt pin.
static byte pinB = 2; // The second hardware interrupt pin.
// Swap the pins if the delay factor decrements if you turn the master control to the right!

// Rotary encoder is wired with the common to ground and the two
// outputs to pins 2 and 3.
Rotary rotary = Rotary(pinA, pinB);

// Rotary Encoder button.
static byte ENC_PUSH = A1;

// All interrupt vars are volatile in order to force the C++ optimiser to leave them alone.
volatile byte aFlag = 0;      // Let's us know when we're expecting a rising edge on pinA to signal that the encoder has arrived at a detent.
volatile byte bFlag = 0;      // Let's us know when we're expecting a rising edge on pinB to signal that the encoder has arrived at a detent (opposite direction to when aFlag is set).
volatile byte counter[NR_OF_EFFECTS]; // This variable stores the current value of the encoder position for each effect. Change to int or uin16_t instead of byte if you want to record a larger range than 0-255.
volatile byte counter_max[NR_OF_EFFECTS];
volatile byte old_counter[NR_OF_EFFECTS];// Stores the last encoder position value so we can compare to the current reading and see if it has changed (so we know when to print to the serial monitor).
volatile byte reading = 0;   // Somewhere to store the direct values we read from our interrupt pins before checking to see if we have moved a whole detent.
volatile int effect = 0; // This must be an int, to be able to count down past 0.
int old_effect = -1;     
#define LEFT -1
#define RIGHT 1
int count_direction = RIGHT;
volatile bool screen_saver = OFF;
// Create an encoder button.
OneButton button;
volatile boolean select_mode = true;

// By default a  mix of wet and dry signals at the output is allowed.
volatile boolean no_dry_signal = false;
volatile boolean old_no_dry_signal = !no_dry_signal;

#define DISPLAY_WIDTH 128 // OLED display width, in pixels  => 16 characters of width 7 pixel/character
#define DISPLAY_HEIGHT 32 // OLED display height, in pixels => 3 lines of characters

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET   -1 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(DISPLAY_WIDTH, DISPLAY_HEIGHT, &Wire, OLED_RESET);

// Pin Definitions: 
// Delay PWM signals TL072B-1/2.
#define DELAY1 10
#define DELAY2 9

// Switch CD4066
#define SWA 6
#define SWB 7
#define SWC 5
#define SWD 4 

// Status message text double click choice.
const String WD = "W+D"; // Wet and Dry signal.
const String WT = "WET"; // No dry signal.
const String DR = "DRY"; // Only dry signal.

// LED (D2 and D3)
#define LED1_G 1 
#define LED1_R 0
#define LED2_G 5
#define LED2_R 6

// On/Off Foot (not implemented in Time-Warp-O-Matic hardware. So if you want to use this 
// rhis needs to be added to the hardware implementation.)
#define BYPASS_DETECT 11

// Contact Foot switch inserted in dedicated jack input.
#define PEDAL_SWITCH    8

// Reserved port for CV voltage control.
#define CV1 A7

// Jumper on PCB.
#define DEBUG_JUMPER  12

#define COUNTER_POSITION 9 // Position of delay or time value on display.
#define MAX_COUNTER 230    // Maximum delay time.

#define CLEAR_NOT   1
#define CLEAR_LOCAL 2
#define CLEAR_LINE  3

#define MAX_MODE_NAME_LEN 12

// Variables.
// DEBUG_JUMPER variables
int delay_variable = 0;
 
unsigned long delay_currentMillis, delay_previousMillis = 0;  

int direction_up = 1;
unsigned long currentMillis, previousMillis = 0; 
unsigned long startTime;
unsigned long endTime;
unsigned long duration = 200;
byte timerRunning = 0;
bool loopb = false;
byte effect_status = HIGH;

// Effect initialisations.
int psycho_counter = 0;
int chorus_counter = 130;


// Screen saver related stuff.
// stensTimer for 'screensaver'.
StensTimer* stensTimer;
Timer *clsTimer;
Timer *screensaverTimer;

// Define actions
#define CLS_TIMER_ACTION 1
#define SCREENSAVER_TIMER_ACTION 2

// Choose some timeouts and number of repetitions.
#define SCREEN_TIMEOUT 300000L       // Screen saver timeout in milli seconds (HL: 5 minutes).
#define SCREENSAVER_TIMEOUT 60000L   // Every minute a time warp will be visible on the display.
#define SCREENSAVER_REPETITION 86400 // Repeat for one whole day should suffice.

// Interrupt Service Routines
//
void rotate(void) {
  // count_direction determines whether rotating (counter)clock wise will decrement or increment a
  // counter. It will increment a counter if the counter represents a delay time, if it represents
  // a speed, it will decrement the counter. For each effect the direction is determined when the 
  // effect is chosen.
  unsigned char rotation_direction = rotary.process();
  if (rotation_direction == DIR_CCW) {
    if (select_mode == true) {
      effect--; // Choose the previous effect.
      if (effect < 0) {
        effect = NR_OF_EFFECTS - 1; // Wrap around.
      }
    } else {
        counter[effect] += count_direction; // De/Increment the effect's speed or delay parameter
        if (counter[effect] < 1) { 
          counter[effect] = 1;
        } else {
            if (counter[effect] > MAX_COUNTER) {
              counter[effect] = MAX_COUNTER;
            }
          }
      }
  }
  if (rotation_direction == DIR_CW) {
    if (select_mode == true) {
      effect++; // Choose the next effect;
      if (effect > NR_OF_EFFECTS - 1) { 
        effect = 0; // Wrap around.
      }
    } else {
        counter[effect] += -count_direction; // De/Increment the effect's speed or delay parameter
        if (counter[effect] < 1) { 
          counter[effect] = 1;
        } else {      
            if (counter[effect] > MAX_COUNTER) {
              counter[effect] = MAX_COUNTER;
            }
          }
      }
  }
  // Stop old timer and start new countdown.
  if (screen_saver == ON) {
    //stensTimer->deleteTimer(clsTimer);
    stensTimer->deleteTimers();
    clsTimer = stensTimer->setTimer(CLS_TIMER_ACTION, SCREEN_TIMEOUT, 1);
    screensaverTimer = stensTimer->setTimer(SCREENSAVER_TIMER_ACTION, SCREENSAVER_TIMEOUT, SCREENSAVER_REPETITION);
    screen_saver = OFF;
    stensTimer->run();
  }
}

// The Interrupt Service Routine for ENC_PUSH Change Interrupt 1
// This routine will only be called on any signal change on ENC_PUSH (H.L. A1): exactly where we need to check.
ISR(PCINT1_vect) {
  // keep watching the push button:
  noInterrupts(); // Stop interrupts happening before we read pin values.
  button.tick();  // Call tick() to check the state.
  interrupts();
}

void displayText(String line_of_text, int field_length, int row, int column, byte clear_mode = CLEAR_LOCAL, int text_size = 2) {
  // field_length: length of field to display line_of_text in.
  // This part will be erased when clear_local is set to true.
  if (field_length == 0) {
    field_length = line_of_text.length();
  }
  display.setTextColor(SSD1306_WHITE); // Draw white text
  display.cp437(true);                 // Use full 256 char 'Code Page 437' font
  switch(clear_mode){
    case CLEAR_LINE:
      if (field_length * 7 * text_size < display.width()) {
        display.fillRect(8 * column, 8 * row, display.width() - field_length * 7 * text_size, 8 * text_size, SSD1306_BLACK);
      } else {
        display.fillRect(8 * column, 8 * row, display.width(), 8 * text_size, SSD1306_BLACK);
      }    
      break;
    case CLEAR_LOCAL:
      display.fillRect(8 * column, 8 * row, field_length * 7 * text_size, 8 * text_size, SSD1306_BLACK);
      break;  
    case CLEAR_NOT:
      break;  
    default:
      Serial.println("Unknown clear mode");
  }
  display.setCursor(column * 8, row * 8);      
  display.setTextColor(WHITE, BLACK);
  display.print(line_of_text);
  display.display();
}

void encoderClick() {
  // Toggle from selecting an effect type to setting a time constant for the delays used.
  #ifdef DEBUG
    Serial.println("encoder click");
  #endif
  select_mode = !select_mode;
  // Clear line 1 and 2.
  displayText("", 0, 0, 0, CLEAR_LINE, 2);
  displayText("", 0, 1, 0, CLEAR_LINE, 2);
  display.clearDisplay();
  // Force entering effect switch in loop() and update W+D;
  loopb = true;
  old_no_dry_signal = !no_dry_signal;
  screen_saver = OFF;
}

void encoderDoubleClick() { 
  // Toggle between adding the dry signal to the output and not sending the dry input signal to the output mixer.
  #ifdef DEBUG
    Serial.println("encoder double click");
  #endif

  switch(effect) {
    // Wet and Dry signal.
    case SHORT_DELAY1:
    #ifdef DEBUG
      case SHORT_DELAY2:
    #endif
    case DELAY:
    case ECHO1:
    case ECHO2:
    case ECHO3:
    case REVERB:
    case PSYCHO:
      no_dry_signal = !no_dry_signal;
      digitalWrite(SWC, no_dry_signal);
      break;
    // Wet only signal.
    case WOW_NOT_FLUTTER:      
    case TELEVERB:
    case CHORUS:
    case FAST_CHORUS: 
    case DECELERATOR:
    case TELEGRAPH:
    break;
    default:
      Serial.print("1: Unknown effect value: ");
      Serial.println(effect);
  }
  screen_saver = OFF;
}

void setSwitches(byte swa, byte swb, byte swc, byte swd) {
  // Set analog switches of CD4066.
  digitalWrite(SWA, swa);
  digitalWrite(SWB, swb);
  digitalWrite(SWC, swc);
  digitalWrite(SWD, swd);
}

void showSwitchStatus(void) {
  char a = digitalRead(SWA) == LOW ? 'L': 'H';
  char b = digitalRead(SWB) == LOW ? 'L': 'H';
  char c = digitalRead(SWC) == LOW ? 'L': 'H';
  char d = digitalRead(SWD) == LOW ? 'L': 'H';
  char tmp[9];
  sprintf(tmp, "%c%c%c%c ", a, b, c, d);
  displayText((String) tmp, 0, 0, 12, CLEAR_LOCAL, 1);

}

void toggleLed13(void) {
  static byte m = LOW;
  // Reverse the LED value.
  m = !m;
  digitalWrite(13, m);
}

void updateSettingsInEeprom(void) {
  EEPROM.update(EFFECT, effect);
  for (int i = 0; i < NR_OF_EFFECTS; i++) {
    EEPROM.update(COUNTER + i, counter[i]);
  }
  EEPROM.update(NO_DRY_SIGNAL, no_dry_signal);
  toggleLed13();
}

void readSettingsFromEeprom(void) {
  // Read important values from EEPROM
  // First get the effect number used the last time the module was used.
  effect = EEPROM.read(EFFECT);
  if ((effect < 0) or (effect > NR_OF_EFFECTS)) {
    effect = 0;
    Serial.print("Error reading effect value from eeprom.");
    EEPROM.write(EFFECT, effect);
  }

  // Second get the delay time.
  for (int i = 0; i < NR_OF_EFFECTS; i++) {
    counter[i] = EEPROM.read(COUNTER + i);
    if ((counter[i] < 0) or (counter[i] > MAX_COUNTER)) {
      Serial.print("Error reading counter[");
      Serial.print(i);
      Serial.println("] value from eeprom.");
      counter[i] = 100;
      EEPROM.write(COUNTER + i, counter[i]);
    }
  }

  // Finally get the state of the no_dry_signal variable.
  byte b = EEPROM.read(NO_DRY_SIGNAL);
  no_dry_signal = (b == 0) ? false: true;
  if ((no_dry_signal == false) or (no_dry_signal == true)) {
    old_no_dry_signal = !no_dry_signal;
  } else {
    no_dry_signal = false;
    EEPROM.write(NO_DRY_SIGNAL, no_dry_signal);
    Serial.print("Error reading no_dry_signal value from eeprom.");
  }
}

void setDelays(unsigned int delay_time1, unsigned int delay_time2) {
  // Set PWM on/off ratio for TL072B-1/2.
  analogWrite(DELAY1, delay_time1);
  analogWrite(DELAY2, delay_time2);
}

void showFillCircle1(void) {
  for (int16_t i = 0; i < DISPLAY_WIDTH / 2; i += 3) {
    // The INVERSE color is used so circles alternate white/black
    display.fillCircle(display.width() / 2, display.height() / 2, i, SSD1306_INVERSE);
    // Update screen with each newly-drawn circle
    display.display(); 
    delay(1);
  }
}

void showFillCircle2(void) {
  for (int16_t i = 0; i < DISPLAY_WIDTH / 2; i += 3) {
    // The INVERSE color is used so circles alternate white/black
    display.fillCircle(display.width() / 2, display.height() / 2, i, SSD1306_BLACK);
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
  delay(600);
  display.clearDisplay();
  displayText(F("Warp"), 0, 1, 4);
  delay(600);
  display.clearDisplay();
  displayText(F("-O-"), 0, 1, 5);
  delay(600);
  showFillCircle1();
  showFillCircle2();
  delay(200);
  display.clearDisplay();  
  displayText(F("Matic"), 0, 1, 3);
  delay(600);  
  displayText(pgm_version, 7, 1, 3, CLEAR_LOCAL, 2);
  delay(1200);  
  display.clearDisplay();
  display.display();
}

void setupDisplay() {
  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  while (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3C for 128x32
    Serial.println(F("SSD1306 allocation failed."));
    delay(1000);
  }
  Serial.println(F("SSD1306 allocation succeeded!")); 
  display.clearDisplay();  
  display.setTextSize(2);  // Normal 2:1 pixel scale
}

void debugMode(void) {  
  
  // This function will run continuously as soon as the 
  // jumper is bridged on the PCB, connecting D12 to GND.
  display.setTextSize(1);
  
  delay_variable += 10;
  if (delay_variable > 4000) delay_variable = 0;
  
  // Show delay variable on display
  char tmp[25];
  sprintf(tmp, "DV:%04d", delay_variable);
  displayText(tmp, 7, 1, 0, CLEAR_LOCAL, 1);

  sprintf(tmp, "Rot. delay  %d", counter[effect]);
  displayText(tmp, 15, 2, 0, CLEAR_LOCAL, 1);
  sprintf(tmp, "Rot. effect %d", effect);
  displayText(tmp, 13, 3, 0, CLEAR_LOCAL, 1);
  sprintf(tmp, "Tap %d", digitalRead(PEDAL_SWITCH));
  displayText(tmp, 5, 4, 0, CLEAR_LOCAL);
  
  if (delay_variable < 2000) {
    setDelays(delay_variable >> 3, delay_variable >> 3); // Every 2s.
  } else {
    setDelays(255 - ((delay_variable - 2000) >> 3), 255 - ((delay_variable - 2000) >> 3)); // Every 2s.
  }
  #ifdef DEBUG  
    Serial.print("Tap Detect: ");
    Serial.println(!digitalRead(PEDAL_SWITCH));
  #endif  
  if (digitalRead(PEDAL_SWITCH) == LOW) { // We use a pullup, so when low it is pressed.
    displayText("TP prssd", 12, 1, 7, CLEAR_LINE);
    if (delay_variable < 2000)
      setSwitches(LOW, LOW, LOW, LOW);     // OFF
    else  
      setSwitches(HIGH, HIGH, HIGH, HIGH); // ON
  }
  if (digitalRead(PEDAL_SWITCH) == HIGH) { // We use a pullup, so when high it is not pressed.
    displayText("TP NT prssd", 0, 1, 6, CLEAR_LINE);
    if (delay_variable < 1000)
      setSwitches(LOW, LOW, LOW, LOW);     // OFF
    else if (delay_variable < 2000)
      setSwitches(HIGH, HIGH, HIGH, HIGH); // ON
    else if (delay_variable < 3000)
      setSwitches(LOW, LOW, LOW, LOW);     // OFF
    else   
      setSwitches(HIGH, HIGH, HIGH, HIGH); // ON
  }
  showSwitchStatus();
  button.tick();
}

void cls(Timer* timer){
  int action = timer->getAction();
  switch(action) {
    case CLS_TIMER_ACTION: 
      display.clearDisplay();
      display.display();
      screen_saver = ON;
      break;
    case SCREENSAVER_TIMER_ACTION:
    if (screen_saver == ON) {
      showFillCircle1();
      showFillCircle2();
    }
  }
  #ifdef DEBUG
    Serial.print("Timer call -> Action: ");
    Serial.print(action);
    Serial.print(", Current Time: ");
    Serial.println(millis());
  #endif
}

void setup() {
  // Set serial device, only for debug purposes
  Serial.begin(57600);
  setupDisplay();
  // Set the pins  
  pinMode(DELAY1,   OUTPUT);
  pinMode(DELAY2,   OUTPUT);
  pinMode(SWA,      OUTPUT);
  pinMode(SWB,      OUTPUT);
  pinMode(SWC,      OUTPUT);
  pinMode(SWD,      OUTPUT);
  pinMode(LED1_G,   OUTPUT);
  pinMode(LED1_R,   OUTPUT);
  pinMode(LED2_G,   OUTPUT);
  pinMode(LED2_R,   OUTPUT);
  pinMode(ENC_PUSH, INPUT_PULLUP); // switch of rotary encoder.
  // The bypass detect pin is not connected in the eurorack version,
  // but its functionality is fully implemented, so you can add 
  // a switch from GND to the BYPASS_DETECT pin if you like.
  pinMode(BYPASS_DETECT, INPUT_PULLUP); 
  pinMode(PEDAL_SWITCH,    INPUT_PULLUP);
  pinMode(DEBUG_JUMPER,  INPUT_PULLUP);

  // Get settings from last time using Time-Warp-O-Matic.
  readSettingsFromEeprom();

  // Initialize the PWM outputs for the current effect.
  setDelays(counter[effect], counter[effect]);

  // Set a few max values for the counters.
  counter_max[DECELERATOR] = DECELERATOR_UPDATE_TIME_MAX;
  counter_max[WOW_NOT_FLUTTER] = WOW_NOT_FLUTTER_TIME_MAX;

  // Initialize the CD4066 switches.
  // Allow some signal to flow into the 2nd tap.
  setSwitches(LOW, HIGH, LOW, LOW);
  
  // Initialize ports for rotary encoder
  pinMode(pinA, INPUT_PULLUP); // Set pinA as an input, pulled HIGH to the logic voltage (5V or 3.3V for most cases)
  pinMode(pinB, INPUT_PULLUP); // Set pinB as an input, pulled HIGH to the logic voltage (5V or 3.3V for most cases)

  // Set hardware interrupts for rotary encoder.
  attachInterrupt(0, rotate, CHANGE);
  attachInterrupt(1, rotate, CHANGE);

  // Attach methods to button clicks.
  button = OneButton(ENC_PUSH, true);
  button.attachClick(encoderClick);
  button.attachDoubleClick(encoderDoubleClick);
  
  // Attach interrupt service routine to ENC_PUSH and call button.tick in order to get
  // a fast response for 'encoder click' and 'encoder double click' events.
  PCICR |= (1 << PCIE1);    // This enables Pin Change Interrupt 1 that covers the Analog input pins or Port C.
  PCMSK1 |= (1 << PCINT9);  // This enables the interrupt for pin 1 of Port C: This is A1.

  // Initialize old counter values.
  for (int i = 0; i < NR_OF_EFFECTS; i++) {
    old_counter[i] = counter[i];
  }

  #ifndef DEBUG  
    // Show Splash screen but only when not in debug mode (to make debugging
    // less tedious).
    showSplashScreen();
  #endif

  // Initialize a timer for the screen saver.
  stensTimer = StensTimer::getInstance();
  // Tell StensTimer which callback function to use.
  stensTimer->setStaticCallback(cls);
  // Set a timer which calls cls after some time.
  clsTimer = stensTimer->setTimer(CLS_TIMER_ACTION, SCREEN_TIMEOUT, 1);
  // Set up a repeating timer which shows the time warp once in a while if the screensaver is on.
  screensaverTimer = stensTimer->setTimer(SCREENSAVER_TIMER_ACTION, SCREENSAVER_TIMEOUT, SCREENSAVER_REPETITION);
}

float speed_factor(void) { 
  // Return a float between 1.0 and 2.0
  return random(100, 401) / 100.0; 
}

void loop() {
  static int delta = 1;
  static float delta_speed = 1.0;
  byte td = 0;
  
  // Detect if the effect is in DEBUG MODE.
  while((digitalRead(DEBUG_JUMPER) == LOW)) {
    debugMode();  
    displayText("Mode: debug", MAX_MODE_NAME_LEN, 0, 0, CLEAR_LOCAL);
  }  
  
  // Detect if the effect is on or off.
  while((digitalRead(BYPASS_DETECT) == LOW)) {
    // Going into bypass mode. To hear anything the W/D potentiometer should be turned to W.
    setSwitches(LOW, LOW, HIGH, LOW);
    display.setTextSize(1);
    displayText("Mode: bypass", MAX_MODE_NAME_LEN, 0, 0, CLEAR_LINE);
    effect_status = LOW;
  } 

  display.setTextSize(2);
  effect_status = HIGH;
  // If in settings mode, show the parameter's name and value (if applicable).
  if (select_mode == false and screen_saver == OFF) {
    switch(effect) {
      case(CHORUS):
      case(FAST_CHORUS):
        displayText("Speed:", 0, 0, 0, CLEAR_LINE, 2);
        displayText((String) (CHORUS_UPPER_LIMIT - counter[effect]), 0, 0, COUNTER_POSITION + 1, CLEAR_LINE, 2);
        break;
                 
      case (WOW_NOT_FLUTTER):
      case(DECELERATOR):      
        displayText("Speed:", 0, 0, 0, CLEAR_LINE, 2);
        displayText((String) (counter_max[effect] - counter[effect]), 0, 0, COUNTER_POSITION + 1, CLEAR_LINE, 2);
        break;

      case (REVERB):
        displayText("Time:", 0, 0, 0, CLEAR_LINE, 2);
        displayText((String) counter[effect], 0, 0, COUNTER_POSITION + 1, CLEAR_LINE, 2);
        break;
                
      default:
        displayText("Time:", 0, 0, 0, CLEAR_LINE, 2);
        displayText((String) (MAX_COUNTER - counter[effect]), 0, 0, COUNTER_POSITION + 1, CLEAR_LINE, 2);
        break;
    }
  }

  // This is the main effect loop.
  // loopb should be set to 'true' for an effect if:
  // 1: the effect depends on the 'PEDAL_SWITCH' button
  // 2: the effect uses its own counter 'counter[effect]'.

  if (effect != old_effect) {
    DECELERATOR_only_once = true;
  }
  
  if ((effect != old_effect) || 
      (counter[effect] != old_counter[effect]) ||
      (loopb == true)) {
    switch(effect)
    {

     case(FAST_CHORUS):
       delta = 2;
       if ((select_mode == true) and (screen_saver == OFF)) displayText("Chorus+", MAX_FX_NAME_LEN, 0, 0, CLEAR_LINE);
       // fall through into case(CHORUS)
       
     case(CHORUS):
       delta = 2;
       if ((effect == CHORUS) and (select_mode == true) and (screen_saver == OFF)) displayText("Chorus", MAX_FX_NAME_LEN, 0, 0, CLEAR_LINE);
       loopb = true;   
       setSwitches(HIGH, LOW, !digitalRead(PEDAL_SWITCH), HIGH);       
       if (effect != old_effect) {
         updateSettingsInEeprom();
       }
       if (direction_up == 1) {
         if (chorus_counter > CHORUS_UPPER_LIMIT) {
             chorus_counter = CHORUS_UPPER_LIMIT; 
             direction_up = 0;
         }
         // Delay created by millis();
         delay_currentMillis = millis();
         if (delay_currentMillis - delay_previousMillis > (MIN_TIME + counter[CHORUS] >> 1)) {
           delay_previousMillis = delay_currentMillis;
           chorus_counter += delta; // If too fast try divider.
         }           
       } else { 
           if (chorus_counter < CHORUS_LOWER_LIMIT) {
             chorus_counter = CHORUS_LOWER_LIMIT;
             direction_up = 1;
           }
           // Delay created by millis();
           delay_currentMillis = millis();
           if (delay_currentMillis - delay_previousMillis > (MIN_TIME + counter[CHORUS] >> 1)) {
             delay_previousMillis = delay_currentMillis;
             chorus_counter -= delta; // If too fast try divider.
           }
       }
       setDelays(chorus_counter, 440 - chorus_counter);
       count_direction = RIGHT;
     break; 

     case(DECELERATOR): 
       if ((select_mode == true) and (screen_saver == OFF)) {
        displayText("Deceleratr", MAX_FX_NAME_LEN, 0, 0, CLEAR_LINE);
       }
       count_direction = RIGHT;
       loopb = true;
       if (effect != old_effect) {
         updateSettingsInEeprom();
         setSwitches(LOW, LOW, LOW, LOW);
       }
       if (digitalRead(PEDAL_SWITCH) == HIGH) {
         DECELERATOR_only_once = true;
       }
       if (DECELERATOR_only_once == true) {
         if (digitalRead(PEDAL_SWITCH) == LOW) {
           setSwitches(LOW, HIGH, LOW, LOW);
           DECELERATOR_only_once = false;
         } else {
           setSwitches(LOW, LOW, LOW, LOW);   
         }
         DECELERATOR_counter = DECELERATOR_counter_max;
       }
       if (counter[DECELERATOR] > DECELERATOR_UPDATE_TIME_MAX) counter[DECELERATOR] = DECELERATOR_UPDATE_TIME_MAX;
       if (counter[DECELERATOR] < DECELERATOR_UPDATE_TIME_MIN) counter[DECELERATOR] = DECELERATOR_UPDATE_TIME_MIN;
       // If TAP footswitch is pushed, reduce the delay time (DECELERATOR_counter) c.q. "slow down" the signal.
       if ((digitalRead(PEDAL_SWITCH) == LOW) && (DECELERATOR_counter >= DECELERATOR_counter_min)) {  
         // The lines below introduce a delay, so the delay time (DECELERATOR_counter) is not reduced too fast.  
         delay_currentMillis = millis();
         if (delay_currentMillis - delay_previousMillis >= counter[DECELERATOR]) {
           delay_previousMillis = delay_currentMillis;
           DECELERATOR_counter--;
         }
         if ((DECELERATOR_counter == DECELERATOR_counter_min) and (digitalRead(PEDAL_SWITCH) == LOW)) {
           setSwitches(LOW, LOW, LOW, LOW);
         }
       }

       // Update the delay time.
       setDelays(DECELERATOR_counter, DECELERATOR_counter);
     break; 
  
     case(SHORT_DELAY1):
       if (select_mode == true) displayText("Short dly", MAX_FX_NAME_LEN, 0, 0, CLEAR_LINE);
       loopb = false;
       setSwitches(HIGH, LOW, no_dry_signal, LOW);
       setDelays(counter[SHORT_DELAY1], counter[SHORT_DELAY1]);
       updateSettingsInEeprom();
       count_direction = RIGHT;
     break; 

     #ifdef DEBUG
       // This delay should be the same as SHORT_DELAY1. If it is not, then
       // something is wrong with the 2nd PT2399 board or its PWM signal.
       // This case is only usable for debugging the hardware.
       case(SHORT_DELAY2):
         if (select_mode == true) displayText("Short dly2", MAX_FX_NAME_LEN, 0, 0, CLEAR_LINE);
         loopb = false;
         setSwitches(LOW, LOW, no_dry_signal, HIGH);
         setDelays(counter[SHORT_DELAY2], counter[SHORT_DELAY2]);
         updateSettingsInEeprom();
         count_direction = RIGHT;
       break; 
     #endif

     case(DELAY):
       if (select_mode == true) displayText("Delay", MAX_FX_NAME_LEN, 0, 0, CLEAR_LINE);
       loopb = false;
       // Do not include the tap 1 signal directly in the output.
       setSwitches(LOW, HIGH, no_dry_signal, LOW);
       setDelays(counter[DELAY], counter[DELAY]);
       updateSettingsInEeprom();
       count_direction = RIGHT;
     break; 
  
     case(ECHO1):
       if (select_mode == true) displayText("Echo", MAX_FX_NAME_LEN, 0, 0, CLEAR_LINE);
       loopb = false;
       // Feed forward the dry signal to the 2nd tap.
       setSwitches(LOW, HIGH, no_dry_signal, HIGH);
       setDelays(counter[ECHO1], counter[ECHO1]);
       updateSettingsInEeprom();
       count_direction = RIGHT;
     break; 

     case(ECHO2):
       if (select_mode == true) displayText("Echo+", MAX_FX_NAME_LEN, 0, 0, CLEAR_LINE);
       loopb = false;
       // Do not feed forward the dry signal to the 2nd tap.
       setSwitches(HIGH, HIGH, no_dry_signal, LOW);
       setDelays(counter[ECHO2], counter[ECHO2]);
       updateSettingsInEeprom();
       count_direction = RIGHT;
     break;        

     case(ECHO3):
       if (select_mode == true) displayText("Echo++", MAX_FX_NAME_LEN, 0, 0, CLEAR_LINE);
       loopb = false;
       // Include the 'middle tap' signal directly in the output as well.
       setSwitches(HIGH, HIGH, no_dry_signal, HIGH);
       setDelays(counter[ECHO3], counter[ECHO3]);
       updateSettingsInEeprom();
       count_direction = RIGHT;
     break; 
         
     case(REVERB):
       if (select_mode == true) displayText("Reverb", MAX_FX_NAME_LEN, 0, 0, CLEAR_LINE);
       loopb = false;
       setSwitches(HIGH, LOW, no_dry_signal, HIGH);
       setDelays(MAX_COUNTER, MAX_COUNTER - (counter[REVERB] >> 1)); // One delay is 1/2 the other.
       updateSettingsInEeprom();
       count_direction = LEFT;
       break; 
  
     case(TELEGRAPH): 
       if ((select_mode == true) and (screen_saver == OFF)) displayText("Telegraph", MAX_FX_NAME_LEN, 0, 0, CLEAR_LINE);
       loopb = true;     
       //setSwitches(LOW, LOW, !digitalRead(PEDAL_SWITCH), LOW); // original
       setSwitches(!digitalRead(PEDAL_SWITCH), LOW, HIGH, LOW);
       if (effect != old_effect) {
         updateSettingsInEeprom();
       }    
       count_direction = LEFT;
     break; 

     case(TELEVERB): // switch reverbed signal on using the tap key.
       if ((select_mode == true) and (screen_saver == OFF)) displayText("TeleVerb", MAX_FX_NAME_LEN, 0, 0, CLEAR_LINE);
       loopb = true;
       td = !digitalRead(PEDAL_SWITCH);       
       setSwitches(td, LOW, td, td);
       setDelays(220, 220 - (counter[REVERB] >> 1));       
       if (effect != old_effect) {
         updateSettingsInEeprom();
       }    
       count_direction = LEFT;
     break; 

     case(WOW_NOT_FLUTTER): 
       if ((select_mode == true) and (screen_saver == OFF)) displayText("WowNotFlut", MAX_FX_NAME_LEN, 0, 0, CLEAR_LINE);
       loopb = true;  
       if (effect != old_effect) {
         updateSettingsInEeprom();
         setSwitches(HIGH, LOW, LOW, LOW);
         delta = random(1, 11);
         delta_speed = random(100, 401) / 100.0; 
         WOW_NOT_FLUTTER_counter = WOW_NOT_FLUTTER_counter_min;
       }
       // counter[WOW_NOT_FLUTTER] determines the speed of changing the delay time.
       // WOW_NOT_FLUTTER_DELTA determines the step size of the change in delay time.
       delay_currentMillis = millis();
       if (delay_currentMillis - delay_previousMillis >= delta_speed * counter[WOW_NOT_FLUTTER]) {
         delay_previousMillis = delay_currentMillis;
         WOW_NOT_FLUTTER_counter += delta;
         if (WOW_NOT_FLUTTER_counter < WOW_NOT_FLUTTER_counter_min) {
           // Choose a positive value for delta.
           delta = random(0, 5);
           delta_speed = (counter[WOW_NOT_FLUTTER] / WOW_NOT_FLUTTER_counter_max) * random(100, 401) / 100.0; 
         } else {
           if (WOW_NOT_FLUTTER_counter > WOW_NOT_FLUTTER_counter_max) {
             // Choose a negative value for delta.
             delta = -random(0, 5);    
             delta_speed = (counter[WOW_NOT_FLUTTER] / WOW_NOT_FLUTTER_counter_max) * random(100, 401) / 100.0; 
           }
         }
       }
       // Update the delay time.
       setDelays(WOW_NOT_FLUTTER_counter * 2, WOW_NOT_FLUTTER_counter * 2);
       count_direction = LEFT;
     break;
  
     case(PSYCHO): 
       if ((select_mode == true) and (screen_saver == OFF)) displayText("Psycho", MAX_FX_NAME_LEN, 0, 0, CLEAR_LINE);
       loopb = true;
       setSwitches(HIGH, HIGH, no_dry_signal, HIGH);       
       if (effect != old_effect) {
         updateSettingsInEeprom();
       }
       if (direction_up == 1) {
         psycho_counter++; // If too fast try divider. 
         if (psycho_counter > 220) {
           psycho_counter = 220; 
           direction_up = 0;
         }
         delay(counter[PSYCHO] >> 1);
       } else {
           psycho_counter--; // If too fast try divider.
           if (psycho_counter < 50) {
             psycho_counter = 20;
             direction_up = 1;
           }
           delay(counter[PSYCHO] >> 1);
       }            
       if ((psycho_counter >> 1) > 50)
         analogWrite(DELAY1, psycho_counter >> 1); 
       else 
         analogWrite(DELAY1, 50);
       analogWrite(DELAY2, 270 - psycho_counter);
       count_direction = LEFT;
     break; 
         
     default:
       #ifdef DEBUG       
         Serial.println("Unknown effect value");
       #endif
       effect = NR_OF_EFFECTS;
       break;
    }
    old_effect = effect;
    old_counter[effect] = counter[effect];
  } 

  // Update wet/dry status in display only when it has changed.
  if ((old_no_dry_signal != no_dry_signal) or (digitalRead(PEDAL_SWITCH) == HIGH)) {
    display.setTextSize(1);
    old_no_dry_signal = no_dry_signal;
    if (screen_saver == OFF) { // Only show the status when the screen saver is off.
      switch(effect) {
        case SHORT_DELAY1:
        #ifdef DEBUG
          case SHORT_DELAY2:
        #endif
        case DELAY:
        case ECHO1:
        case ECHO2:
        case ECHO3:
        case REVERB:
        case PSYCHO:
          if (no_dry_signal == false) // Only wet signal on output.
            displayText((String) WT, 4, 3, MAX_FX_NAME_LEN + 1, CLEAR_LOCAL, 1);
          else // Wet and dry signal on output.
            displayText((String) WD, 4, 3, MAX_FX_NAME_LEN + 1, CLEAR_LOCAL, 1);
          break;
        case TELEVERB:
          if (digitalRead(PEDAL_SWITCH) == HIGH) {
            displayText((String) WD, 4, 3, MAX_FX_NAME_LEN + 1, CLEAR_LOCAL, 1);
          }
          break;
        case DECELERATOR:
        case CHORUS:
        case FAST_CHORUS: // Wet only.
        case WOW_NOT_FLUTTER:
        case TELEGRAPH:        
          displayText((String) WT, 4, 3, MAX_FX_NAME_LEN + 1, CLEAR_LOCAL, 1);
        break;
        default:
          Serial.print("2: Unknown effect value: ");
          Serial.println(effect);
      }
      display.setTextSize(2);
    }
  }
  button.tick();
  stensTimer->run();
}
