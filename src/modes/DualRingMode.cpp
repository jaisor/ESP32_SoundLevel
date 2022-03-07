#include "DualRingMode.h"

#define OUTTER_RING_SIZE 141

#define S1C1 0xFFD700
#define S1C2 0x665700
#define S2C1 0x0057B8
#define S2C2 0x003066

extern const TProgmemRGBPalette16 S1_p FL_PROGMEM =
{
    S1C1,
    S1C1,
    S1C1,
    S1C1,

    S1C2,
    S1C2,
    S1C2,
    S1C2,

    S1C1,
    S1C1,
    S1C1,
    S1C1,
    
    S1C2,
    S1C2,
    S1C2,
    S1C2,
};

extern const TProgmemRGBPalette16 S2_p FL_PROGMEM =
{
    S2C1,
    S2C1,
    S2C1,
    S2C1,

    S2C2,
    S2C2,
    S2C2,
    S2C2,

    S2C1,
    S2C1,
    S2C1,
    S2C1,
    
    S2C2,
    S2C2,
    S2C2,
    S2C2,
};

CDualRingMode::CDualRingMode(const uint16_t numLeds, const String name)
: CBaseMode(numLeds, name), increment(255.0 / (float)numLeds), blendType(LINEARBLEND), delay(15) {
    Log.noticeln("Mode '%s', increment: '%D'", name, increment);
    segments.push_back(CLEDSegment(0, OUTTER_RING_SIZE * 0.25, S1_p));
    segments.push_back(CLEDSegment(OUTTER_RING_SIZE * 0.25, OUTTER_RING_SIZE * 0.75, S2_p));
    segments.push_back(CLEDSegment(OUTTER_RING_SIZE * 0.75, OUTTER_RING_SIZE, S1_p));
    segments.push_back(CLEDSegment(OUTTER_RING_SIZE, OUTTER_RING_SIZE + (numLeds - OUTTER_RING_SIZE) * 0.25, S1_p));
    segments.push_back(CLEDSegment(OUTTER_RING_SIZE + (numLeds - OUTTER_RING_SIZE) * 0.25, OUTTER_RING_SIZE + (numLeds - OUTTER_RING_SIZE) * 0.75, S2_p));
    segments.push_back(CLEDSegment(OUTTER_RING_SIZE + (numLeds - OUTTER_RING_SIZE) * 0.75, numLeds, S1_p));
}

void CDualRingMode::draw(CRGB *leds) {

    if (millis() - tMillis > configuration.ledDelayMs) {
        tMillis = millis();
        startIndex = startIndex + 1;
    }

/*
    float ci = startIndex;
    for( uint16_t i = 0; i < numLeds; i++) {
        leds[i] = ColorFromPalette( palette, (uint8_t)ci, 255, blendType);
        ci+=increment;
    }
*/

    float ci = startIndex;
    for(CLEDSegment s : segments) {
        for( uint16_t i = s.start; i < s.end; i++) {
            leds[i] = ColorFromPalette( s.palette, (uint8_t)ci, 255, blendType);
            ci+=increment;
        }
    }
}