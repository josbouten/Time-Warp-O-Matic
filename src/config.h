#ifndef __CONFIG_H
    #define __CONFIG_H

    #define PGM_VERSION F("v0.5")

    // Enable WRITE_TO_EEPROM to store settings in eeprom
    #define WRITE_TO_EEPROM

    // When using the device the 1st time, make sure the eeprom is
    // filled from address 0:
    // 1: define INIT_EEPROM
    // 2: compile and run once
    // 3: un-define INIT_EEPROM
    // 4: compile and run as often as you like.

    // Screen saver related stuff.
    #define SCREENSAVER

    // Use DEBUG to show some output of variables to test the software.
    // Note, this may stop the allocation of the dsiplay, so when using this
    // you will only see debug print statements on the serial port.
    //#define DEBUG

    // Use the builtin LED to signal the MPU is running.
    //#define ALIVE

    #define COUNTER_INIT_VALUE 100

    // For test purposes: use builtin led to show there is an external clock.
    // If so, use symbolic times to select the delay time.
    // The presence of the clock will double the blink frequency.
    // If we have not received a clock signal for CLOCK_TIMEOUT milli seconds,
    // we will revert to delay in an integer value from 0 ... 255.
    #define EXT_CLOCK_TIMEOUT 2000000L // Time in mS before we conclude the clock signal was removed.
    #define DIV_FACTOR 1000

    // Uncomment the following line if you want to pybass all delay units
    // and test the analog path only.
    // To hear anything the W/D potentiometer should be turned to W.
    // #define BYPASS_MODE

    #define ON true
    #define OFF false

    // Effects definitions.
    #define DECELERATOR      0
    #define SHORT_DELAY      1
    #define DELAY            2
    #define ECHO1            3
    #define ECHO2            4
    #define ECHO3            5
    #define CHORUS           6
    #define FAST_CHORUS      7
    #define REVERB           8
    #define WOW_NOT_FLUTTER  9
    #define TELEGRAPH       10
    #define TELEVERB        11
    #define PSYCHO          12

    #define NR_OF_EFFECTS   13

    #define INITIAL_EFFECT SHORT_DELAY
    #define DELAY_TIME_MIN 7

    // EEPROM message memory locations.
    #define NO_DRY_SIGNAL (NR_OF_EFFECTS + 1)

    // Chorus time constants.
    #define MIN_TIME 80
    #define CHORUS_LOWER_LIMIT 185
    #define CHORUS_UPPER_LIMIT 250

    #define MAX_FX_NAME_LEN 12

    #define LEFT -1
    #define RIGHT 1

    #define DISPLAY_WIDTH 128 // OLED display width, in pixels  => 16 characters of width 7 pixel/character
    #define DISPLAY_HEIGHT 32 // OLED display height, in pixels => 3 lines of characters

    // pin definitions
    // Delay PWM signals TL072B-1/2.
    #define DELAY1 10 // D10
    #define DELAY2 9  //  D9

    // Switch CD4066
    #define SWA 6 // D6
    #define SWB 7 // D7
    #define SWC 5 // D5
    #define SWD 4 // D4

    // Status message text double click choice.
    #define WD F("W+D") // Wet and Dry signal.
    #define WT F("WET") // No dry signal.
    #define DR F("DRY") // Only dry signal.

    // On/Off Foot (not implemented in Time-Warp-O-Matic hardware. So if you want to use this
    // rhis needs to be added to the hardware implementation.)
    #define BYPASS_DETECT 11 // D11

    // Foot switch on input jack "P".
    #define PEDAL_SWITCH   8 // D8

    // Input for external clock.
    #define CV1 A3

    #define COUNTER_POSITION     7 // Position of delay or time value on display.
    #define MAX_COUNTER        255 // Maximum delay time.
    #define NOTES_STR_POSITION   7

    #define CLEAR_NOT   1
    #define CLEAR_LOCAL 2
    #define CLEAR_LINE  3

    #ifdef SCREENSAVER
        #define SCREENCLS_DELAY 3000000L // Time in milli seconds
        #define SCREENSAVER_DELAY 60000 // Time in milli seconds
        #define CLS_TIMER_ACTION 2
        #define SCREENSAVER_TIMER_ACTION 4
    #endif

    #define NR_OF_CYCLES 3 // We compute the mean time of 5 cycles as the cycle time.

    #define DELAY_TIME_BEFORE_WRITING_TO_EEPROM_IN_MS 10000 // Time in milli seconds.

    #define NR_OF_MULT_FACTORS 16

    #define INITIAL_BASE_FACTOR 7
    #define INITIAL_DELAY_TIME 220

    #define LED_DELAY 1000 // 1 mS

    #define isChorusOrReverb (settings.effect == CHORUS || settings.effect == FAST_CHORUS || settings.effect == REVERB)

#endif