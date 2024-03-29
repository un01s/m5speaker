#include <M5UnitLCD.h>
#include <M5UnitOLED.h>
#include <M5Unified.h>

#include "A2DP_Speaker.hpp"
#include "Simple_FFT.hpp"
#include "Palette.hpp"

#include <esp_log.h>

/// set ESP32-A2DP device name
static constexpr char bt_device_name[] = "ESP32SPK";

static wb::A2DP_Speaker a2dp_sink = { &M5.Speaker, 0 };
static wb::Simple_FFT fft;
static wb::Palette palette;

/// need better way to do this
#define FFT_SIZE 256

static constexpr size_t WAVE_SIZE = 320;
static bool fft_enabled = false;
static bool wave_enabled = false;
static uint16_t prev_y[(FFT_SIZE / 2)+1];
static uint16_t peak_y[(FFT_SIZE / 2)+1];
static int16_t wave_y[WAVE_SIZE];
static int16_t wave_h[WAVE_SIZE];
static int16_t raw_data[WAVE_SIZE * 2];
static int header_height = 0;

/// GFX stuff to handle the display
uint32_t bgcolor(LGFX_Device* gfx, int y)
{
  auto h = gfx->height();
  auto dh = h - header_height;
  int v = ((h - y)<<5) / dh;
  if (dh > 44)
  {
    int v2 = ((h - y - 1)<<5) / dh;
    if ((v >> 2) != (v2 >> 2))
    {
      return 0x666666u;
    }
  }
  return gfx->color888(v + 2, v, v + 6);
}

void gfxSetup(LGFX_Device* gfx)
{
  if (gfx == nullptr) { return; }
  if (gfx->width() < gfx->height())
  {
    gfx->setRotation(gfx->getRotation()^1);
  }
  gfx->setFont(&fonts::lgfxJapanGothic_12);
  gfx->setEpdMode(epd_mode_t::epd_fastest);
  gfx->setCursor(0, 8);
  gfx->print("BT A2DP : ");
  gfx->println(bt_device_name);
  gfx->setTextWrap(false);
  gfx->fillRect(0, 6, gfx->width(), 2, TFT_BLACK);

  header_height = (gfx->height() > 80) ? 45 : 21;
  fft_enabled = !gfx->isEPD();
  if (fft_enabled)
  {
    wave_enabled = (gfx->getBoard() != m5gfx::board_M5UnitLCD);

    for (int y = header_height; y < gfx->height(); ++y)
    {
      gfx->drawFastHLine(0, y, gfx->width(), bgcolor(gfx, y));
    }
    // all the background lines are drawn.
  }

  for (int x = 0; x < (FFT_SIZE/2)+1; ++x)
  {
    prev_y[x] = INT16_MAX;
    peak_y[x] = INT16_MAX;
  }
  for (int x = 0; x < WAVE_SIZE; ++x)
  {
    wave_y[x] = gfx->height();
    wave_h[x] = 0;
  }
}

void gfxLoop(LGFX_Device* gfx)
{
  if (gfx == nullptr) { return; }
  if (header_height > 32)
  {
    auto bits = a2dp_sink.getMetaUpdateInfo();
    if (bits)
    {
      gfx->startWrite();
      for (int id = 0; id < a2dp_sink.metatext_num; ++id)
      {
        if (0 == (bits & (1<<id))) { continue; }
        size_t y = id * 12;
        if (y+12 >= header_height) { continue; }
        gfx->setCursor(4, 8 + y);
        gfx->fillRect(0, 8 + y, gfx->width(), 12, gfx->getBaseColor());
        gfx->print(a2dp_sink.getMetaData(id, true));
        gfx->print(" "); // Garbage data removal when UTF8 characters are broken in the middle.
      }
      gfx->display();
      gfx->endWrite();
    }
  }
  else
  {
    static int title_x;
    static int title_id;
    static int wait = INT16_MAX;

    if (a2dp_sink.getMetaUpdateInfo())
    {
      gfx->fillRect(0, 8, gfx->width(), 12, TFT_BLACK);
      a2dp_sink.clearMetaUpdateInfo();
      title_x = 4;
      title_id = 0;
      wait = 0;
    }

    if (--wait < 0)
    {
      int tx = title_x;
      int tid = title_id;
      wait = 3;
      gfx->startWrite();
      uint_fast8_t no_data_bits = 0;
      do
      {
        if (tx == 4) { wait = 255; }
        gfx->setCursor(tx, 8);
        const char* meta = a2dp_sink.getMetaData(tid, false);
        if (meta[0] != 0)
        {
          gfx->print(meta);
          gfx->print("  /  ");
          tx = gfx->getCursorX();
          if (++tid == a2dp_sink.metatext_num) { tid = 0; }
          if (tx <= 4)
          {
            title_x = tx;
            title_id = tid;
          }
        }
        else
        {
          if ((no_data_bits |= 1 << tid) == ((1 << a2dp_sink.metatext_num) - 1))
          {
            break;
          }
          if (++tid == a2dp_sink.metatext_num) { tid = 0; }
        }
      } while (tx < gfx->width());
      --title_x;
      gfx->display();
      gfx->endWrite();
    }
  }

  if (!gfx->displayBusy())
  { // draw volume bar
    static int px;
    uint8_t v = M5.Speaker.getVolume();
    int x = v * (gfx->width()) >> 8;
    if (px != x)
    {
      gfx->fillRect(x, 6, px - x, 2, px < x ? 0xAAFFAAu : 0u);
      gfx->display();
      px = x;
    }
  }

  if (fft_enabled && !gfx->displayBusy())
  {
    static int prev_x[2];
    static int peak_x[2];
    static bool prev_conn;
    bool connected = a2dp_sink.is_connected();
    if (prev_conn != connected)
    {
      prev_conn = connected;
      if (!connected)
      {
        a2dp_sink.clear();
      }
    }

    auto buf = a2dp_sink.getBuffer();
    if (buf)
    {
      // WAVE_SIZE = 320
      // data int16_t, stereo = 2 channels
      // buf is from A2DP sink
      // raw_data is used for preprocessing like sampling and FFT 
      memcpy(raw_data, buf, WAVE_SIZE * 2 * sizeof(int16_t)); // stereo data copy
      gfx->startWrite();

      // draw stereo level meter
      // 2 means 2 channels for stereo
      for (size_t i = 0; i < 2; ++i)
      {
        int32_t level = 0;
        // sampling here for stereo level meter
        // 640/32 = 20 samples, then find the max value from 20 samples
        for (size_t j = i; j < 640; j += 32)
        {
          uint32_t lv = abs(raw_data[j]);
          if (level < lv) { level = lv; }
        }
        // level is the max of 20 samples of one channel
        // normalize to the width
        // each channel has the height of 2 pixels
        // first channel starts at 0 of y
        // second channel starts at 3 of y
        // use different color based on previous max to current max, px < x
        int32_t x = (level * gfx->width()) / INT16_MAX;
        int32_t px = prev_x[i];
        if (px != x)
        {
          gfx->fillRect(x, i * 3, px - x, 2, px < x ? 0xFF9900u : 0x330000u);
          prev_x[i] = x;
        }
        // peak_x is the max of all maxium values
        px = peak_x[i];
        if (px > x)
        {
          gfx->writeFastVLine(px, i * 3, 2, TFT_BLACK);
          px--;
        }
        else
        {
          px = x;
        }
        if (peak_x[i] != px)
        {
          peak_x[i] = px;
          gfx->writeFastVLine(px, i * 3, 2, TFT_WHITE);
        }
      }
      gfx->display();

      // draw FFT level meter
      // run FFT first
      fft.exec(raw_data);
      // bw = 5 = 320/60
      size_t bw = gfx->width() / 60;
      if (bw < 3) { bw = 3; }
      int32_t dsp_height = gfx->height();
      int32_t fft_height = dsp_height - header_height - 1;
      size_t xe = gfx->width() / bw; // 320/5= 64
      if (xe > (FFT_SIZE/2)) { xe = (FFT_SIZE/2); }

      int32_t wave_next = ((header_height + dsp_height) >> 1) + (((256 - (raw_data[0] + raw_data[1])) * fft_height) >> 17);

      uint32_t bar_color[2] = { 0x000033u, 0x99AAFFu };

      // xe = 64, this is the bin number, each bin has the width of 5
      for (size_t bx = 0; bx <= xe; ++bx)
      {
        size_t x = bx * bw;
        if ((x & 7) == 0) { gfx->display(); taskYIELD(); }
        int32_t f = fft.get(bx);
        int32_t y = (f * fft_height) >> 18;
        if (y > fft_height) { y = fft_height; }
        ///Serial.printf("b=%d,y=%d\n",bx, y);

        y = dsp_height - y;
        int32_t py = prev_y[bx];
        if (y != py)
        {
          gfx->fillRect(x, y, bw - 1, py - y, bar_color[(y < py)]);
          //gfx->fillRect(x, y, bw - 1, py - y, palette.get_color(y+100));
          prev_y[bx] = y;
        }
        py = peak_y[bx] + 1;
        if (py < y)
        {
          gfx->writeFastHLine(x, py - 1, bw - 1, bgcolor(gfx, py - 1));
          //gfx->writeFastHLine(x, py - 1, bw - 1, palette.get_color(py+100));
        }
        else
        {
          py = y - 1;
        }
        if (peak_y[bx] != py)
        {
          peak_y[bx] = py;
          //gfx->writeFastHLine(x, py, bw - 1, TFT_WHITE);
          gfx->writeFastHLine(x, py, bw - 1, TFT_RED);
          //gfx->writeFastHLine(x, py, bw - 1, TFT_YELLOW);
        }
        // y=1074364467, py=1073522256
        //Serial.printf("y=%d, py=%d\n");
        
        //Serial.printf("fft_height = %d\n", fft_height);
        // ftt_height = 194 = 240 - 45(header_height) - 1

        // last to draw to wave form of the input
        if (wave_enabled)
        {
          for (size_t bi = 0; bi < bw; ++bi)
          {
            size_t i = x + bi;
            if (i >= gfx->width() || i >= WAVE_SIZE) { break; }
            y = wave_y[i];
            int32_t h = wave_h[i];
            bool use_bg = (bi+1 == bw);
            if (h>0)
            { /// erase previous wave.
              gfx->setAddrWindow(i, y, 1, h);
              h += y;
              do
              {
                uint32_t bg = (use_bg || y < peak_y[bx]) ? bgcolor(gfx, y)
                            : (y == peak_y[bx]) ? 0xFFFFFFu
                            : bar_color[(y >= prev_y[bx])];
                gfx->writeColor(bg, 1);
              } while (++y < h);
            }
            size_t i2 = i << 1;
            int32_t y1 = wave_next;
            wave_next = ((header_height + dsp_height) >> 1) + (((256 - (raw_data[i2] + raw_data[i2 + 1])) * fft_height) >> 17);
            int32_t y2 = wave_next;
            if (y1 > y2)
            {
              int32_t tmp = y1;
              y1 = y2;
              y2 = tmp;
            }
            y = y1;
            h = y2 + 1 - y;
            wave_y[i] = y;
            wave_h[i] = h;
            if (h>0)
            { /// draw new wave.
              gfx->setAddrWindow(i, y, 1, h);
              h += y;
              do
              {
                uint32_t bg = (y < prev_y[bx]) ? 0xFFCC33u : 0xFFFFFFu;
                gfx->writeColor(bg, 1);
              } while (++y < h);
            }
          }
        }
      }
      gfx->display();
      gfx->endWrite();
    }
  }
}

/// arduion setup()
void setup(void)
{  
  auto cfg = M5.config();
#if defined ( ARDUINO )
  cfg.serial_baudrate = 115200;   // default=9600. if "Serial" is not needed, set it to 0.
#endif

  M5.begin(cfg);

  // setup the log output level
  M5_LOGD("M5_LOGD info log");

  { /// custom setting
    auto spk_cfg = M5.Speaker.config();
    /// Increasing the sample_rate will improve the sound quality instead of increasing the CPU load.
    spk_cfg.sample_rate = 96000; // default:64000 (64kHz)  e.g. 48000 , 50000 , 80000 , 96000 , 100000 , 128000 , 144000 , 192000 , 200000
    spk_cfg.task_pinned_core = APP_CPU_NUM;
    // spk_cfg.task_priority = configMAX_PRIORITIES - 2;
    spk_cfg.dma_buf_count = 20;
    // spk_cfg.dma_buf_len = 512;
    M5.Speaker.config(spk_cfg);
  }
  M5.Speaker.begin();

  a2dp_sink.start(bt_device_name, false);

  Serial.printf("header_height = %d\n", header_height); // 0
  gfxSetup(&M5.Display);
  Serial.printf("header_height = %d\n", header_height); // 45

  // for testing
  LGFX_Device* gfx = &M5.Display;
  Serial.printf("width = %d\n", gfx->width());
  Serial.printf("height = %d\n", gfx->height());
  Serial.printf("bin width = %d\n", (size_t)(gfx->width() / 60));
}

/// arduino loop() 
void loop(void)
{
  gfxLoop(&M5.Display);

  {
    static int prev_frame;
    int frame;
    do
    {
      vTaskDelay(1);
    } while (prev_frame == (frame = millis() >> 3)); /// 8 msec cycle wait
    prev_frame = frame;
  }

  M5.update();
  if (M5.BtnA.wasPressed())
  {
    M5.Speaker.tone(440, 50);
  }
  if (M5.BtnA.wasDeciedClickCount())
  {
    switch (M5.BtnA.getClickCount())
    {
    case 1:
      M5.Speaker.tone(1000, 100);
      a2dp_sink.next();
      break;

    case 2:
      M5.Speaker.tone(800, 100);
      a2dp_sink.previous();
      break;
    }
  }
  if (M5.BtnA.isHolding() || M5.BtnB.isPressed() || M5.BtnC.isPressed())
  {
    size_t v = M5.Speaker.getVolume();
    int add = (M5.BtnB.isPressed()) ? -1 : 1;
    if (M5.BtnA.isHolding())
    {
      add = M5.BtnA.getClickCount() ? -1 : 1;
    }
    v += add;
    if (v <= 255)
    {
      M5.Speaker.setVolume(v);
    }
  }
}

#if !defined ( ARDUINO )
extern "C" {
  void loopTask(void*)
  {
    setup();
    for (;;) {
      loop();
    }
    vTaskDelete(NULL);
  }

  void app_main()
  {
    xTaskCreatePinnedToCore(loopTask, "loopTask", 8192, NULL, 1, NULL, 1);
  }
}
#endif
