#ifndef _EEPROM
#define _EEPROM

/*
  January 2026, code by Jos Bouten

  Eeprom-O-Matic

  Important: the eeprom does not need to be erased to use this class (and what
  does erasing it mean anyway?).

  This code was written to be used with an arduino nano or similar but
  it may work on other devices. You mileage may vary. Use at your own peril.

  Settings in a struct can be written to or read form the eeprom in one go.
  So there is no need for knowing or changing the address to save the
  variables to (if the settings struct is all you have to deal with).
  This class limits reusing a specific piece of the eeprom.
  This is done by writing a unique marker preceding the data.
  The marker can be used to find the data and to determine
  the next location for writing data. In this way the address space
  is used evenly.

  eeprom.read(): reading from the eeprom starts by searching the marker and
  then reading the settings struct. The marker is defined as 0x66666666
  (four times 66 in a row). If you expect this number of sixes to consecutively be
  part of your settings then choose a more unique marker or reorder the variables
  in the struct. The same goes for the ERASE_MARKER. The EMPTY_MARKER
  is used only once when no data has been written to the eeprom before.
  eeprim.write(): the data is written to a new part of the eeprom each time
  the write function is called.

  ATTENTION ATTENTION ATTENTION ATTENTION ATTENTION ATTENTION
  ===========================================================
  The one strict rule you must adhere to is that the size of the settings struct
  must be a multiple of the size of the marker (which is 4 bytes). Add dummy
  bytes to the settings struct when necessary.
  ===========================================================
  ATTENTION ATTENTION ATTENTION ATTENTION ATTENTION ATTENTION

  The maximum amount of data that can be written to the eeprom is the
  eeprom size - sizeof(DATA_MARKER)
*/

#define DATA_MARKER ((unsigned long) 0x66666666 )          // The DATA_MARKER measures 4 bytes in size.
#define ERASE_MARKER ((unsigned long) 0x33333333 )
#define EMPTY_MARKER ((unsigned long) 0x22222222 )
#define CNT_FOR_LINE_BREAK 8

#define SIZE_OF_DATA_MARKER sizeof(DATA_MARKER)

// Note, the size of the SettingsObjType struct in bytes must be an integer
// multiple of the size of DATA_MARKER !

#include <EEPROM.h>

#include "config.h"

class Eeprom {
    private:
        char msg[80];
        // Start address of memory struct in eeprom.
        unsigned int  eeprom_currentAddress;
        // Size of EEPROM in bytes.
        unsigned int  eeprom_sizeInBytes;
        unsigned long eeprom_dataItem = 0;
        bool blocked = false;

        // Print a data element and increase the print count.
        // Meant to be used for debug purposes only.
        void printData(unsigned long data, unsigned int address, int &printCnt) {
            if ((printCnt % CNT_FOR_LINE_BREAK) == 0) {
                sprintf(msg, "\n%04d -> ", address);
                Serial.print(msg);
            }
            // Highlight the DATA_MARKER so that it is easy to see
            // were the most recent data was written in the eeprom.
            if (eeprom_dataItem == DATA_MARKER) {
                sprintf(msg, ">%08lx<", data);
            } else {
                sprintf(msg, " %08lx ", data);
            }
            Serial.print(msg);
            printCnt++;
        }

        // Get rid of the settings marker by overwriting it.
        // That marker should always precede the current settings data.
        void eraseMarkerByte() {
            // The marker is always at the readAddress.
            EEPROM.put(eeprom_currentAddress, ERASE_MARKER);
        }

    public:

        Eeprom() {}

        Eeprom(unsigned int length): eeprom_sizeInBytes(length) {
            // Find or set the eeprom_currentAddress.
            init();
        }

        // This function is used for debug purposes only.
        // There is no need to erase the eeprom other than seeing what data
        // is written to it while running debug experiments.
        void erase() {
            Serial.print("Erasing eeprom.\n");
            for (unsigned int address = 0; address < eeprom_sizeInBytes; address++) {
                EEPROM.write(address, 0xff); // This could be any value.
            }
            eeprom_currentAddress = 0;
            EEPROM.put(eeprom_currentAddress, EMPTY_MARKER);
        }

        // Return the size of the eeprom in bytes.
        int getSize() {
            return(eeprom_sizeInBytes);
        }

        // Print an error message to serial out.
        void printError() {
            Serial.print(F("\nERROR: the size of the settings struct: "));
            Serial.print(SIZE_OF_SETTINGS_STRUCT);
            Serial.print(F(" MUST be an integer multiple of the size of the DATA_MARKER:"));
            Serial.print(SIZE_OF_DATA_MARKER);
            uint8_t amount = SIZE_OF_DATA_MARKER - (SIZE_OF_SETTINGS_STRUCT % SIZE_OF_DATA_MARKER);
            Serial.print(F("ERROR: add "));
            Serial.print(amount);
            Serial.println(F("dummy byte"));
            if (amount > 1) {
                Serial.print("s ");
            }
            Serial.print(F("to the struct to remedy this."));
        }

        // Initialize the eeprom class.
        // Find the DATA_MARKER and store its address in eeprom_currentAddress OR
        // prepare eeprom for first write.
        void init() {
            if ((SIZE_OF_SETTINGS_STRUCT % SIZE_OF_DATA_MARKER) != 0) {
                printError();
                blocked = true;
                return;
            }
            if ((SIZE_OF_SETTINGS_STRUCT + SIZE_OF_DATA_MARKER) > eeprom_sizeInBytes) {
                sprintf(msg, "Data chunk size (+ marker): ");
                Serial.print(msg);
                sprintf(msg, "is too large for this EEPROM's size: %d", eeprom_sizeInBytes);
                Serial.print(msg);

                eeprom_currentAddress = 0;
                return;
            }
            // Look for marker byte in eeprom and store its memory address.
            // This is the address preceding the data chunk that was stored in the eeprom the last time.
            for (eeprom_currentAddress = 0;
                 eeprom_currentAddress < eeprom_sizeInBytes - SIZE_OF_SETTINGS_STRUCT;
                 eeprom_currentAddress += SIZE_OF_DATA_MARKER) {
                EEPROM.get(eeprom_currentAddress, eeprom_dataItem);
                if (eeprom_dataItem == DATA_MARKER) {
                    // We found a marker which should be followed by an empty eeprom or by a settings object.
                    Serial.print(F("Eeprom::Init found start address for reading at:"));
                    Serial.println(eeprom_currentAddress);
                    return;
                }
            }
            // If we get to here, no data marker bytes were found. So we are dealing with an EEPROM this class
            // has not seen before.
            eeprom_currentAddress = 0;
            EEPROM.put(eeprom_currentAddress, EMPTY_MARKER);
            // sprintf(msg, "Init found start address for reading at: %d\n", eeprom_currentAddress);
            // Serial.print(msg);
        }

        // Check whether the eeprom is 'empty' or not.
        // Empty means that no data as defined in the settings struct has been written to it using this class yet
        // but the eeprom has been initialised.
        bool isEmpty() {
            unsigned long dataItem;
            EEPROM.get(eeprom_currentAddress, dataItem);
            if (dataItem == EMPTY_MARKER) {
                return(true);
            } else {
                return(false);
            }
        }

        // Return the address to the most recently written data or 0 if no data
        // has been written to the eeprom yet.
        unsigned int getCurrentAddress() {
            return(eeprom_currentAddress);
        }

        // Write data to the EEPROM.
        // Returns the number of data bytes written including the size of the marker.
        long write(SettingsObjType settings) {
            if (blocked) {
                printError();
                return(-1);
            } else {
                EEPROM.get(eeprom_currentAddress, eeprom_dataItem);
                // We need to erase the marker at the current address now.
                eraseMarkerByte();
                if (eeprom_dataItem == EMPTY_MARKER) {
                    EEPROM.put(eeprom_currentAddress, DATA_MARKER);
                    EEPROM.put(eeprom_currentAddress + SIZE_OF_DATA_MARKER, settings);
                } else {
                    // Calculate and check the new start address for writing the data.
                    unsigned int tmpAddress = eeprom_currentAddress + SIZE_OF_SETTINGS_STRUCT + SIZE_OF_DATA_MARKER;
                    if (tmpAddress > eeprom_sizeInBytes - 1) {
                        // There is insufficient room near the end of the eeprom.
                        Serial.print("Can not write data past the end of the EEPROM, will try to begin at address 0.\n");
                        eeprom_currentAddress = 0;
                    } else {
                        eeprom_currentAddress = tmpAddress;
                    }
                    // Write new marker followed by the data.
                    EEPROM.put(eeprom_currentAddress, DATA_MARKER);
                    EEPROM.put(eeprom_currentAddress + SIZE_OF_DATA_MARKER, settings);
                }
                return(SIZE_OF_SETTINGS_STRUCT + SIZE_OF_DATA_MARKER);
            }
        }

        // Read the data chunk follwing the startAddress.
        // Will return the number of data bytes read or -1.
        long read(SettingsObjType *p) {
            if (blocked) {
                printError();
                return(-1);
            } else {
                if ((eeprom_currentAddress + SIZE_OF_SETTINGS_STRUCT) > eeprom_sizeInBytes) {
                    Serial.print("Eeprom too small for the supplied data structure.\n");
                    sprintf(msg, "Data structure (size=%d) must not exceed %d bytes.\n", SIZE_OF_SETTINGS_STRUCT, eeprom_sizeInBytes - SIZE_OF_DATA_MARKER);
                    Serial.print(msg);
                    return -1;
                }
                EEPROM.get(eeprom_currentAddress, eeprom_dataItem);
                if (eeprom_dataItem == DATA_MARKER) {
                    // The data is stored immediately following the marker.
                    EEPROM.get(eeprom_currentAddress + SIZE_OF_DATA_MARKER, *p);
                    return(SIZE_OF_SETTINGS_STRUCT);
                } else {
                    // We did not find a DATA_MARKER. So this is an eeprom that was not written to by this class.
                    return(-1);
                }
            }
        }

        // Print the content of the Eeprom as raw bytes.
        // Meant to be used for debug purposes only.
        #ifdef DEBUG1
            void printContent() {
                if (blocked) {
                    printError();
                } else {
                    int cnt = 0;
                    sprintf(msg, "Content of %d eeprom addresses:", eeprom_sizeInBytes);
                    Serial.print(msg);
                    for (unsigned int nextAddress = 0; nextAddress < eeprom_sizeInBytes; nextAddress += SIZE_OF_DATA_MARKER) {
                        EEPROM.get(nextAddress, eeprom_dataItem);
                        printData(eeprom_dataItem, nextAddress, cnt);
                    }
                    Serial.print(" EOF\n");
                }
            }
        #endif

        #ifdef PRIME_THE_EEPROM
            // Write a struct to the start of the eeprom.
            // Can be used to prime / initialise an eeprom with some data.
            void prime(SettingsObjType settings) {
                if (blocked) {
                    printError();
                } else {
                    EEPROM.get(eeprom_currentAddress, eeprom_dataItem);
                    // We need to erase the marker at the current address now.
                    eraseMarkerByte();
                    // Now write the settings at the beginning of the eeprom.
                    EEPROM.put(0, DATA_MARKER);
                    EEPROM.put(SIZE_OF_DATA_MARKER, settings);
                }
            }
        #endif
};
#endif