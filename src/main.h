#ifndef __MAIN_H
#define __MAIN_H

#include <Arduino.h>

typedef struct SettingsObjType {
  volatile byte delayTime[NR_OF_EFFECTS];
  volatile bool isWetAndDrySelected;
  volatile int8_t effect = INITIAL_EFFECT; // This must be an int, to be able to count down past 0.0;
  // The baseFactorIndex keeps track of the position in the baseFactor vector an effect has been set to.
  volatile byte baseFactorIndex[NR_OF_EFFECTS];
  //uint32_t dummy1 = 0xff00ff00;  // Add dummy bytes until total size of struct is integer multiple of sizeof(MARKER)
} SettingsObjType_t ;

SettingsObjType_t settings;

#define SIZE_OF_SETTINGS_STRUCT sizeof(SettingsObjType)

#endif