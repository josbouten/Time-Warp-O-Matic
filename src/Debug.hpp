#ifndef __DEBUG_H
    #define __DEBUG_H

    #define AVR_BOARD // for AVR-based arduino boards
    //#define ARM_BOARD // for ARM based arduino boards

    #ifdef DEBUG

        #ifdef LIBPRINTF
            // use libprintf
            #include "LibPrintf.h"
            #define debug_begin(z) Serial.begin(z)
            #define debug_print(z) printf(z)
            #define debug_print2(z, y) printf(z, y)
            #define debug_print3(z, y, x) printf(z, y, x)
            #define debug_print4(z, y, x, w) printf(z, y, x, w)
            #define debug_print5(z, y, x, w, v) printf(z, y, x, w, v)
            #define debug_print6(z, y, x, w, v, u) printf(z, y, x, w, v, u)
            #define debug_print7(z, y, x, w, v, u, t) printf(z, y, x, w, v, u, t)
            #define debug_print8(z, y, x, w, v, u, t, s) printf(z, y, x, w, v, u, t, s)

            #ifdef AVR_BOARD
            int freeRam() { // measure SRAM usage in AVR-based arduino boards
                extern int __heap_start,*__brkval;
                int v;
                return (int)&v - (__brkval == 0  ? (int)&__heap_start : (int) __brkval);
            }
            #endif
            #ifdef ARM_BOARD // measure SRAM usage in ARM-based arduino boards
                extern "C" char* sbrk(int incr);
                int freeRam() {
                    char top;
                    return &top - reinterpret_cast<char*>(sbrk(0));
                }
            #endif
            void print_freeram() {
                printf("SRAM left: %d\n", freeRam());
            }
        #else
            // User serial print
            #define debug_begin(z) Serial.begin(z);
            #define debug_print_ncr(z) Serial.print(z);
            #define debug_print(z) Serial.println(z);
            #define debug_print2(z, y) { Serial.print(z); Serial.println(y); }
            #define debug_print3(z, y, x) { Serial.print(z); Serial.print(y); Serial.println(x); }
            #define debug_print4(z, y, x, w) { Serial.print(z); Serial.print(y); Serial.print(x), Serial.println(w); }
            #define debug_print6(z, y, x, w, v, u) { Serial.print(z); Serial.print(y); Serial.print(x), Serial.print(w); Serial.print(v); Serial.println(u); }
        #endif
    #else
        #define debug_begin(z)
        #define debug_print(z)
        #define debug_print_ncr(z)
        #define debug_print2(z, y)
        #define debug_print3(z, y, x)
        #define debug_print4(z, y, x, w)
        #define debug_print5(z, y, x, w, v)
        #define debug_print6(z, y, x, w, v, u)
        #define debug_print7(z, y, x, w, v, u, t)
        #define debug_print8(z, y, x, w, v, u, t, s)
    #endif

#endif