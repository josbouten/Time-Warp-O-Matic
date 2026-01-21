#ifndef __MILLIS
#define __MILLIS

#include <Arduino.h>

class MillisDelay {

    private:
        unsigned long _delayTime;
        unsigned long _oldTime;
        bool _alreadyFinished = false;
        bool _stop = false;

    public:
        MillisDelay() {}

        MillisDelay(unsigned long delayTime):
            _delayTime(delayTime), _oldTime(millis()) { }

        void start() {
            _oldTime = millis();
            _alreadyFinished = false;
        }

        // Return true as soon and as long as the delay has finished.
        bool justFinished() {
            if (_stop) {
                _stop = false;
                return(true);
            } else {
                return((millis() - _oldTime) > _delayTime);
            }
        }

        // Set the delay time in milli seconds.
        void set(unsigned long someDelayTime) {
            _delayTime = someDelayTime;
        }
};

#endif