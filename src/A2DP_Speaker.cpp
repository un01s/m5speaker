#include "A2DP_Speaker.hpp"

namespace wb {

const int16_t* A2DP_Speaker::getBuffer(void) {
  return _tri_buf[_export_index];
}

const char* A2DP_Speaker::getMetaData(size_t id, bool clear_flag = true) {
  if (clear_flag) { 
    _meta_bits &= ~(1<<id); 
  } 
  return (id < metatext_num) ? _meta_text[id] : nullptr;
}

uint8_t A2DP_Speaker::getMetaUpdateInfo(void) {
  return _meta_bits;
}

void A2DP_Speaker::clearMetaUpdateInfo(void) {
  _meta_bits = 0;
}

void A2DP_Speaker::clear(void) {
  for (int i = 0; i < 3; ++i)
  {
    if (_tri_buf[i]) { 
      memset(_tri_buf[i], 0, _tri_buf_size[i]); 
    }
  }
}

void A2DP_Speaker::clearMetaData(void) {
  for (int i = 0; i < metatext_num; ++i)
  {
    _meta_text[i][0] = 0;
  }
  _meta_bits = (1<<metatext_num)-1;
}

void A2DP_Speaker::av_hdl_a2d_evt(uint16_t event, void *p_param)
{
    esp_a2d_cb_param_t* a2d = (esp_a2d_cb_param_t *)(p_param);

    switch (event) {
    case ESP_A2D_CONNECTION_STATE_EVT:
      if (ESP_A2D_CONNECTION_STATE_CONNECTED == a2d->conn_stat.state)
      { // 接続

      }
      else
      if (ESP_A2D_CONNECTION_STATE_DISCONNECTED == a2d->conn_stat.state)
      { // 切断

      }
      break;

    case ESP_A2D_AUDIO_STATE_EVT:
      if (ESP_A2D_AUDIO_STATE_STARTED == a2d->audio_stat.state)
      { // 再生

      } else
      if ( ESP_A2D_AUDIO_STATE_REMOTE_SUSPEND == a2d->audio_stat.state
        || ESP_A2D_AUDIO_STATE_STOPPED        == a2d->audio_stat.state )
      { // 停止
        clearMetaData();
        clear();
      }
      break;

    case ESP_A2D_AUDIO_CFG_EVT:
      {
        esp_a2d_cb_param_t *a2d = (esp_a2d_cb_param_t *)(p_param);
        size_t tmp = a2d->audio_cfg.mcc.cie.sbc[0];
        size_t rate = 16000;
        if (     tmp & (1 << 6)) { rate = 32000; }
        else if (tmp & (1 << 5)) { rate = 44100; }
        else if (tmp & (1 << 4)) { rate = 48000; }
        _sample_rate = rate;
      }
      break;

    default:
      break;
    }

    BluetoothA2DPSink::av_hdl_a2d_evt(event, p_param);
}

void A2DP_Speaker::av_hdl_avrc_evt(uint16_t event, void *p_param)
{
    esp_avrc_ct_cb_param_t *rc = (esp_avrc_ct_cb_param_t *)(p_param);

    switch (event)
    {
    case ESP_AVRC_CT_METADATA_RSP_EVT:
      for (size_t i = 0; i < metatext_num; ++i)
      {
        if (0 == (rc->meta_rsp.attr_id & (1 << i))) { continue; }
        strncpy(_meta_text[i], (char*)(rc->meta_rsp.attr_text), metatext_size);
        _meta_bits |= rc->meta_rsp.attr_id;
        break;
      }
      break;

    case ESP_AVRC_CT_CONNECTION_STATE_EVT:
      break;

    case ESP_AVRC_CT_CHANGE_NOTIFY_EVT:
      break;

    default:
      break;
    }

    BluetoothA2DPSink::av_hdl_avrc_evt(event, p_param);
}

int16_t* A2DP_Speaker::get_next_buf(const uint8_t* src_data, uint32_t len)
{
    size_t tri = _tri_index < 2 ? _tri_index + 1 : 0;
    if (_tri_buf_size[tri] < len)
    {
      _tri_buf_size[tri] = len;
      if (_tri_buf[tri] != nullptr) { heap_caps_free(_tri_buf[tri]); }
      auto tmp = (int16_t*)heap_caps_malloc(len, MALLOC_CAP_8BIT);
      _tri_buf[tri] = tmp;
      if (tmp == nullptr)
      {
        _tri_buf_size[tri] = 0;
        return nullptr;
      }
    }
    memcpy(_tri_buf[tri], src_data, len);
    _tri_index = tri;
    return _tri_buf[tri];
}

void A2DP_Speaker::audio_data_callback(const uint8_t *data, uint32_t length)
{
    // Reduce memory requirements by dividing the received data into the first and second halves.
    length >>= 1;
    M5.Speaker.playRaw(get_next_buf( data        , length), length >> 1, _sample_rate, true, 1, virtual_channel);
    M5.Speaker.playRaw(get_next_buf(&data[length], length), length >> 1, _sample_rate, true, 1, virtual_channel);
    _export_index = _tri_index;
}

} // end of namespace wb
