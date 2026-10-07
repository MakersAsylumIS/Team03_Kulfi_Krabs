// TEST A4 — Codec input: record 5 s from the board's OWN onboard mics to the SD card, then play it back
// Connect: USB (UART) + headphones in PHONE jack + microSD (FAT32) in the slot, DIP switches CMD and DATA3 ON.
// Libraries: arduino-audio-tools + arduino-audio-driver.
// Pass:    after "Playing back" you hear what you said. Proves record AND playback with no external parts.

#include "AudioTools.h"                                   // audio framework
#include "AudioTools/AudioLibs/AudioBoardStream.h"        // Audio Kit / ES8388 driver
#include "SD_MMC.h"                                       // ESP32's built-in SD card driver (SDMMC bus, not SPI)

AudioBoardStream kit(AudioKitEs8388V1);                   // the board / codec object

void setup() {                                            // runs once; the whole test lives here
  Serial.begin(115200);                                   // open serial
  delay(300);                                             // settle
  Serial.println("TEST A4: onboard mic record + playback");  // banner

  if (!SD_MMC.begin("/sdcard", true)) {                   // mount the SD card; `true` = 1-bit mode (leaves GPIO13/KEY2 free)
    Serial.println("SD mount FAILED - card in? DIP CMD+DATA3 ON? FAT32?");  // explain the likely cause
    return;                                               // stop the test here
  }

  // ---------- RECORD ----------
  auto rx = kit.defaultConfig(RX_MODE);                   // config for receive (recording) mode
  rx.sd_active = false;                                   // the driver must not touch SD pins - we handle SD ourselves
  rx.input_device = ADC_INPUT_LINE2;                      // LINE2 = the onboard microphones on the v2.2 board
  rx.sample_rate = 16000;                                 // 16 kHz is plenty for voice and keeps files small
  rx.channels = 1;                                        // mono
  rx.bits_per_sample = 16;                                // 16-bit samples
  kit.begin(rx);                                          // start the codec in record mode

  SD_MMC.remove("/rec.wav");                              // delete any old recording so the file starts fresh
  File f = SD_MMC.open("/rec.wav", FILE_WRITE);           // open the output file for writing
  WAVEncoder enc;                                         // encoder that writes a proper WAV header + raw samples
  EncodedAudioStream out(&f, &enc);                       // output stream = file + WAV encoder
  out.begin(rx);                                          // tell the encoder the sample format so the header is right
  StreamCopy rec(out, kit);                               // copier: from the codec (mic) into the WAV file

  Serial.println("Recording 5 seconds - talk at the board now");  // prompt the tester
  unsigned long t0 = millis();                            // remember the start time
  while (millis() - t0 < 5000) {                          // for five seconds...
    rec.copy();                                           // ...keep moving mic samples into the file
  }
  out.end();                                              // finalise the WAV header with the real length
  f.close();                                              // close the file so it is safely on the card
  kit.end();                                              // stop the codec so we can restart it in playback mode

  // ---------- PLAY BACK ----------
  Serial.println("Playing back...");                      // tell the tester what is happening
  f = SD_MMC.open("/rec.wav");                            // reopen the recording for reading
  auto tx = kit.defaultConfig(TX_MODE);                   // config for transmit (playback) mode
  tx.sd_active = false;                                   // again, driver leaves SD pins alone
  tx.sample_rate = 16000;                                 // must match the recording
  tx.channels = 1;                                        // mono, as recorded
  tx.bits_per_sample = 16;                                // 16-bit, as recorded
  kit.begin(tx);                                          // start the codec in playback mode
  kit.setVolume(0.8);                                     // 80 % volume
  WAVDecoder dec;                                         // decoder that reads the WAV header and hands out samples
  EncodedAudioStream in(&f, &dec);                        // input stream = file + WAV decoder
  in.begin();                                             // parse the header
  StreamCopy play(kit, in);                               // copier: from the file into the codec (headphones)
  while (play.copy()) {                                   // copy until the file is finished (copy() returns 0 at the end)
  }
  f.close();                                              // close the file
  kit.end();                                              // stop the codec
  SD_MMC.end();                                           // unmount the card
  Serial.println("Done. PASS if you heard yourself clearly.");  // final verdict line
}

void loop() {                                             // nothing to do after setup()
  delay(1000);                                            // idle
}
