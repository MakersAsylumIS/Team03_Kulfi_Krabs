// TEST A3 — Codec output: play a 440 Hz tone on the PHONE (3.5 mm) jack
// Connect: USB (UART) + any headphones in the PHONE jack.
// Libraries: arduino-audio-tools and arduino-audio-driver (both by pschatzmann, via Library Manager).
// Pass:    a clean continuous tone in the headphones. Silence = codec / I2C fault -> use the spare board.

#include "AudioTools.h"                                   // the audio framework (streams, generators, copy loop)
#include "AudioTools/AudioLibs/AudioBoardStream.h"        // driver layer that knows the Audio Kit's ES8388 codec and pins

AudioBoardStream kit(AudioKitEs8388V1);                   // the board object: v2.2 boards marked ES8388 use the "V1" pin map
SineWaveGenerator<int16_t> sine(12000);                   // a sine-wave source; 12000 = amplitude (max is 32767)
GeneratedSoundStream<int16_t> sound(sine);                // wraps the generator so it can be read like a stream
StreamCopy copier(kit, sound);                            // copies audio from `sound` (source) into `kit` (the codec output)

void setup() {                                            // runs once
  Serial.begin(115200);                                   // open serial
  delay(300);                                             // let it settle
  Serial.println("TEST A3: 440 Hz tone on the PHONE jack");  // say what we are doing
  auto cfg = kit.defaultConfig(TX_MODE);                  // start from the board's default config, in transmit (playback) mode
  cfg.sd_active = false;                                  // we are not using the SD card in this test
  cfg.sample_rate = 44100;                                // CD-quality sample rate
  cfg.channels = 2;                                       // stereo (the jack is stereo; both sides get the tone)
  cfg.bits_per_sample = 16;                               // 16-bit samples
  kit.begin(cfg);                                         // initialise the codec over I2C and the I2S bus with that config
  kit.setVolume(0.6);                                     // 60 % volume - loud enough, safe for headphones
  sine.begin(cfg, 440);                                   // tell the generator the sample format and the frequency (440 Hz = A4)
}

void loop() {                                             // runs forever
  copier.copy();                                          // move one buffer of sine samples into the codec; keeps the tone going
}
