#include <Arduino.h>
#include <algorithm>
#include "Palette.hpp"
#include "Equalizer.hpp"

#undef min

namespace wb {

Equalizer::Equalizer(Palette *palette, int x, int y, int width, int height, int num_bins) : Component(x, y, width, height)
{

}

void Equalizer::update(float *mag)
{

}

void Equalizer::_draw(LGFX_Device* gfx)
{

}

}

