// /*
//     I built the ADSR yesterday following the video. Neat! When looking at the code I saw the eeprom
//     read and write functions you used. I thought I'd write a bit about an alternative approach you
//     might care to try in the future. I did this in the form of some code which you should be able
//     to compile and experiment with. Unfortunately I need to split it up as it is too long according to discord

//     Writing lots of variables to eeprom and reading them back is a nuisance while coding
//     because you need to know the byte size of all the variabels and increment the write
//     address accordingly. You need to write them in sequence and read them back in exactly the
//     same sequence. I always felt this was a pain and the approach it is very error prone.

//     However, there is an alternative way of doing that which you might want to consider.
//     It entails that you bundle all variables in a struct and put() that whole struct into eeprom or
//     get() that struct from eeprom. You need not know the size of the variables in the struct.
//     The compiler knows their byte size of the struct so the struct can be written in one go and can
//     be read in one go. There is no need for you to know in which sequence they are put in the eeprom,
//     the compiler solves that for you as it writes and reads the struct as one block of bytes.
//     The contents of the struct are however available as the member variables.
//     Have a look at the example code:
//     You should be able to paste this text in an IDE and compile it.
// */

// #include <Arduino.h>
// #include <EEPROM.h>

// // We define a struct containing all the vars we want to store.

// // some constants
// #define NR_OF_EFFECTS 10
// #define INITIAL_EFFECT 6

// typedef struct SettingsType {
//   unsigned int attackValueCh1;
//   unsigned int decayValueCh1;
//   unsigned int sustainValueCh1;
//   unsigned int releaseValueCh1;
//   unsigned int attackValueCh2;
//   unsigned int decayValueCh2;
//   unsigned int sustainValueCh2;
//   unsigned int releaseValueCh2;
//   unsigned int attack1Multiplier;
//   // etc etc
//   // All types of variables are allowed here.
//   int delayTime[NR_OF_EFFECTS];
//   bool isWetAndDrySelected;
//   int effect = INITIAL_EFFECT;
//   int baseFactorIndex[NR_OF_EFFECTS];
// } SettingsType ;

// // As you can see, all kinds of variable types can be part of the struct. This includes arrays.

// // In our main code we declare a variable which will be where we keep track of things.
// SettingsType settings;

// // Writing to eeprom will then be nothing more than this:
// void writeDataToEeprom(long startAddress, SettingsType settings) {
//    EEPROM.put(startAddress, settings);
// }

// // Reading from eeprom is also very simple.
// void readDataFromEeprom(long startAddress, SettingsType *settings) {
//    EEPROM.get(startAddress, *settings);
// }

// // If you want to use one of the variables in the struct, you can use them like this:
// void envelopeStartTrig1() {
//   unsigned int gate1Active = 1, envelope1Phase, rampDuration1;
//   //GATE ON
//   if (gate1Active == 1) {
//     if (envelope1Phase == 0) {
//       envelope1Phase = 1;
//       rampDuration1 = settings.attackValueCh1 * settings.attack1Multiplier;  // fixed values in the config file.
//       Serial.print(rampDuration1);
//       // etc etc
//     }
//   }
//   // etc etc
// }
// void setup() {

// }

// void loop() {
//   long address = 0;
//   // Do some stuff.

//   // Writing to and reading from EEPROM can be done like so:

//   writeDataToEeprom(address, settings);
//   // Do some more stuff.
//   readDataFromEeprom(address, &settings);
// }

// /*
//   As you can see, handing the eeprom is now simplified a lot.
//   In your code you now access all the variables which are members of
//   the SettingsType struct via a dot like so:
//   settings.attackValueCh1

//   e.g. in

//   Serial.println(settings.attackValueCh1);

//   The added side effect of using a struct is that all things you consider to be settings
//   and you want to be stored to and read from eeprom are now gathered together in one place
//   and can easily be identified in the code because of the "settings." preceding them.
// */