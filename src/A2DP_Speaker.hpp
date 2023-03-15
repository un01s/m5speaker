#pragma once

#include <Arduino.h>
#include <BluetoothA2DPSink.h>
#include <M5Unified.h>

namespace wb {

class A2DP_Speaker : public BluetoothA2DPSink 
{
public: 
  A2DP_Speaker(m5::Speaker_Class* m5sound, uint8_t virtual_channel)
  : BluetoothA2DPSink()
  {
    is_i2s_output = false; // I2S is not required
    virtual_channel = virtual_channel;
  }

  const int16_t* getBuffer(void);
  const char* getMetaData(size_t id, bool clear_flag);
  uint8_t getMetaUpdateInfo(void);
  void clearMetaUpdateInfo(void);
  void clear(void);

  static constexpr size_t metatext_size = 128;
  static constexpr size_t metatext_num = 3;
  static constexpr uint8_t virtual_channel = 0;

protected:
  int16_t* _tri_buf[3] = { nullptr, nullptr, nullptr };
  size_t _tri_buf_size[3] = { 0, 0, 0 };
  size_t _tri_index = 0;
  size_t _export_index = 0;
  char _meta_text[metatext_num][metatext_size];
  uint8_t _meta_bits = 0;
  size_t _sample_rate = 48000;

  void clearMetaData(void);
  void av_hdl_a2d_evt(uint16_t event, void *p_param) override;
  void av_hdl_avrc_evt(uint16_t event, void *p_param) override;
  int16_t* get_next_buf(const uint8_t* src_data, uint32_t len);
  void audio_data_callback(const uint8_t *data, uint32_t length) override;

};

} // end of namespace wb
