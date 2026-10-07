// TEST F — 28 mm speaker / handset earpiece: 1 kHz tone through the PHONE jack
// Connect: half of the cut aux cable in the PHONE jack; its TIP wire to one speaker terminal, its SLEEVE wire to the other. USB (UART).
// Libraries: arduino-audio-tools + arduino-audio-driver.
// Pass:    a clean 1 kHz tone from the speaker, audible across a room. (If silent, try headphones: if they work, the cable/speaker is at fault.)

#include "AudioTools.h"                                   // audio framework
#include "AudioTools/AudioLibs/AudioBoardStream.h"        // Audio Kit / ES8388 driver

AudioBoardStream kit(AudioKitEs8388V1);                   // board / codec object
SineWaveGenerator<int16_t> sine(16000);                   // sine source, amplitude 16000 (a bit louder than test A3)
GeneratedSoundStream<int16_t> sound(sine);                // generator as a readable stream
StreamCopy copier(kit, sound);                            // copier from the sine into the codec

void setup() {                                            // runs once
  Serial.begin(115200);                                   // open serial
  delay(300);                                             // settle
  Serial.println("TEST F: 1 kHz tone on the PHONE jack for the speaker");  // banner
  auto cfg = kit.defaultConfig(TX_MODE);                  // playback config
  cfg.sd_active = false;                                  // no SD in this test
  cfg.sample_rate = 44100;                                // sample rate
  cfg.channels = 2;                                       // stereo output (tip = left; we only use tip + sleeve)
  cfg.bits_per_sample = 16;                               // 16-bit
  kit.begin(cfg);                                         // start codec
  kit.setVolume(0.8);                                     // 80 % - we want to judge loudness; drop to 0.5 if it distorts
  sine.begin(cfg, 1000);                                  // 1000 Hz
}

void loop() {                                             // runs forever
  copier.copy();                                          // keep feeding the tone to the codec
}
