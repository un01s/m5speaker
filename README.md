# m5core2 speaker

This code is from M5Unified/examples/Advanced/Bluetooth_with_ESP32A2DP. Build this with Platform.io. 

Everything is OK. Just the volume of the M5Core2 seems pretty soft. How to increase the default volume of BT?

The volume can be controlled by three buttons of M5Core2. Holding button A and click button B.

## reference

* [atomic14](https://github.com/atomic14/m5stack-core2-audio-monitor)

This is a bluetooth A2DP speaker. It is also a arduino-based platformio project for M5stack Core2.

The below is the ini file.

```
[env:m5stack-core2]
platform = espressif32
board = m5stack-core2
framework = arduino
monitor_port = /dev/cu.SLAB_USBtoUART
monitor_speed = 115200
upload_speed = 115200
upload_port = /dev/cu.SLAB_USBtoUART
monitor_filters = esp32_exception_decoder
build_flags = -DBOARD_HAS_PSRAM -Ofast
lib_deps = 
  126@3.4.0 ; FastLED
  M5Core2
```

It has diffent code stucture in the UI. It has three different components that can be turned invisible. 

* wave
* FFT equalizer
* spectrum



