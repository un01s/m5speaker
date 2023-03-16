#pragma once

#include "Component.hpp"

namespace wb {

class Palette;

class Equalizer : public Component
{
private:
  Palette *m_palette;
  int m_num_bins;
  float *bar_chart;
  float *bar_chart_peaks;

public:
  Equalizer(Palette *palette, int x, int y, int width, int height, int num_bins);
  void update(float *mag);
  void _draw(LGFX_Device* gfx);
};

}

