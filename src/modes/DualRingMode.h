#pragma once

#include "BaseMode.h"

class CLEDSegment {

public:
    const uint16_t start, end;
    const TProgmemRGBPalette16& palette;

	CLEDSegment(const uint16_t start, const uint16_t end, const TProgmemRGBPalette16& palette)
    : start(start), end(end), palette(palette) {};
};

class CDualRingMode : public CBaseMode {

private:
    uint8_t startIndex = 0;
    const float increment;
    //const TProgmemRGBPalette16& palette1, palette2;
    const TBlendType blendType;
    const unsigned long delay;
    std::vector<CLEDSegment> segments;

public:
	CDualRingMode(const uint16_t numLeds, const String name);
    virtual void draw(CRGB *leds);
};
