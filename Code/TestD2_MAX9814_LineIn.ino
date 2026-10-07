// TEST D2 — MAX9814 microphone module measured through the board's audio codec (no soldering, no GPIO36)
// Connect: MAX9814 VDD -> header 3V3, MAX9814 GND -> header GND,
//          MAX9814 OUT -> TIP wire of the cut aux cable, MAX9814 GND -> SLEEVE wire of that cable,
//          plug that cable into the board's AUX IN / LINE IN jack (the 3.5 mm jack that is NOT the PHONE jack).
//          GAIN and AR on the module: leave unconnected. USB (UART) to the laptop.
// Libraries: arduino-audio-tools + arduino-audio-driver (pschatzmann). Partition: Huge APP.
// Pass:    "swing" small in silence (under ~300) and large when you talk 5 cm away (several thousand), bar grows with your voice.

#include "AudioTools.h"                                     // audio framework
#include "AudioTools/AudioLibs/AudioBoardStream.h"          // Audio Kit / ES8388 codec driver

AudioBoardStream kit(AudioKitEs8388V1);                     // the board / codec object
int16_t buf[512];                                           // room for 512 samples read from the codec at a time

void setup() {                                              // runs once
  Serial.begin(115200);                                     // open serial
  delay(300);                                               // settle
  Serial.println("TEST D2: MAX9814 via the codec LINE IN. Silence, then talk, then snap fingers.");  // instructions
  auto cfg = kit.defaultConfig(RX_MODE);                    // receive (recording) configuration
  cfg.sd_active = false;                                    // not using the SD card
  cfg.input_device = ADC_INPUT_LINE1;                       // LINE1 = the AUX/LINE IN jack on this board (LINE2 = onboard mics)
  cfg.sample_rate = 16000;                                  // 16 kHz is enough for voice
  cfg.channels = 1;                                         // mono: we only wired the tip (left)
  cfg.bits_per_sample = 16;                                 // 16-bit samples, matches int16_t buf
  kit.begin(cfg);                                           // start the codec in record mode
}

void loop() {                                               // runs forever, prints one line per buffer
  size_t n = kit.readBytes((uint8_t*)buf, sizeof(buf));     // pull up to 1024 bytes = 512 samples from the codec
  int count = n / 2;                                        // number of 16-bit samples actually received
  if (count == 0) return;                                   // nothing yet - try again
  int mn = 32767, mx = -32768;                              // trackers for the lowest and highest sample
  long sum = 0;                                             // for the average
  for (int i = 0; i < count; i++) {                         // scan the buffer
    if (buf[i] < mn) mn = buf[i];                           // minimum
    if (buf[i] > mx) mx = buf[i];                           // maximum
    sum += buf[i];                                          // total
  }
  int avg = sum / count;                                    // average (DC offset) - should sit near 0 because the codec blocks DC
  int swing = mx - mn;                                      // peak-to-peak = loudness, on a 0..65535 scale
  Serial.printf("avg %6d  swing %6d  ", avg, swing);        // print the numbers
  for (int i = 0; i < swing / 400; i++) Serial.print('#');  // bar: one # per 400 counts
  Serial.println();                                         // end of line
  delay(100);                                               // readable rate (~10 lines/second)
}
