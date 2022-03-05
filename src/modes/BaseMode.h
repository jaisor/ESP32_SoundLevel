#pragma once

#include <Arduino.h>
#include <FastLED.h>

#include "Configuration.h"

class CBaseMode {

protected:
    unsigned long tMillis;
    const uint16_t numLeds;
    const String name;

public:
	CBaseMode(const uint16_t numLeds, const String name);
    virtual void draw(CRGB *leds) {};

    const String getName() { return name; }
};
