#pragma once

#include <M5Unified.h>

namespace wb {

class Component
{
public:
  int x, y, width, height;
  bool visible;
  Component(int x, int y, int width, int height) : x(x), y(y), width(width), height(height), visible(true)
  {
  }
  virtual void _draw(LGFX_Device* gfx) = 0;
  void draw(LGFX_Device* gfx)
  {
    if (visible)
    {
      _draw(gfx);
    }
  }
};

} // end of namespace wb

