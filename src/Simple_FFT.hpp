#pragma once

#include <Arduino.h>

namespace wb {

#define FFT_SIZE 256
//#define M_PI 3.141592653

class Simple_FFT 
{
  float _wr[FFT_SIZE + 1];
  float _wi[FFT_SIZE + 1];
  float _fr[FFT_SIZE + 1];
  float _fi[FFT_SIZE + 1];
  uint16_t _br[FFT_SIZE + 1];
  size_t _ie;

public:

  Simple_FFT(void);

  void exec(const int16_t* in);

  uint32_t get(size_t index);

};

} // end of namespace wb
