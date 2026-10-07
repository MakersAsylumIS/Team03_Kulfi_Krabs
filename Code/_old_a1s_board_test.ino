/*
  Stranger Phone — ESP32-A1S Audio Kit v2.2 (ES8388) bring-up test
  ------------------------------------------------------------------
  Six stages, chosen from the Serial Monitor (115200 baud):
    1  Serial + LED      - board alive, D4 LED (GPIO22) blinks
    2  Keys              - prints KEY1..KEY6 (GPIO36,13,19,23,18,5) + header pins 21, 22
    3  SD card           - mounts microSD in 1-bit mode, lists files, writes/reads test.txt
    4  Tone out          - 440 Hz sine on the PHONE jack / earpiece for 3 s
    5  Record            - 5 s from the mic (onboard MIC or MAX9814 on MIC1 pads) -> /rec.wav
    6  Play back         - plays /rec.wav
    7  Keep-alive check  - prints current-draw advice; leave running on the power bank overnight

  Libraries (Arduino IDE > Library Manager, or ZIP from GitHub):
    - arduino-audio-tools   (pschatzmann)
    - arduino-audio-driver  (pschatzmann)
  Board: "ESP32 Dev Module", Partition: "Huge APP", Upload speed 921600, port = the micro-USB marked UART.
*/

#include "AudioTools.h"
#include "AudioTools/AudioLibs/AudioBoardStream.h"
#include "SD_MMC.h"
#include <WiFi.h>

// ---- pins on the Audio Kit v2.2 ----
const int KEYS[6]   = {36, 13, 19, 23, 18, 5};   // KEY1..KEY6
const int PIN_HOOK  = 22;                        // header pin, also drives LED D4 (lights when pulled low)
const int PIN_KEY7  = 21;                        // header pin = PA_ENABLE; we never drive it as output
const int PIN_LED   = 22;

AudioBoardStream kit(AudioKitEs8388V1);          // v2.2 boards marked ES8388 use the V1 pin map
SineWaveGenerator<int16_t> sine(12000);
GeneratedSoundStream<int16_t> sound(sine);

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== ESP32-A1S bring-up test ===");
  for (int i = 0; i < 6; i++) pinMode(KEYS[i], INPUT_PULLUP);   // GPIO36 has NO internal pull-up: external 10k to 3V3 required
  pinMode(PIN_HOOK, INPUT_PULLUP);
  pinMode(PIN_KEY7, INPUT_PULLUP);
  menu();
}

void menu() {
  Serial.println("1 LED  2 Keys  3 SD  4 Tone  5 Record  6 Play  7 Keep-alive  -> type a number");
}

void loop() {
  if (!Serial.available()) return;
  char c = Serial.read();
  switch (c) {
    case '1': stageLed();     break;
    case '2': stageKeys();    break;
    case '3': stageSd();      break;
    case '4': stageTone();    break;
    case '5': stageRecord();  break;
    case '6': stagePlay();    break;
    case '7': stageKeepAlive(); break;
    default: return;
  }
  menu();
}

// ---------- 1 ----------
void stageLed() {
  Serial.println("Stage 1: LED D4 should blink 5 times");
  pinMode(PIN_LED, OUTPUT);
  for (int i = 0; i < 5; i++) { digitalWrite(PIN_LED, LOW); delay(200); digitalWrite(PIN_LED, HIGH); delay(200); }
  pinMode(PIN_HOOK, INPUT_PULLUP);                 // give the pin back to the hook switch
  Serial.println("PASS if it blinked. FAIL = board not powered / wrong board selected.");
}

// ---------- 2 ----------
void stageKeys() {
  Serial.println("Stage 2: press each key / short header pins 21 and 22 to GND. 15 seconds. 'x' to stop early.");
  unsigned long t0 = millis();
  while (millis() - t0 < 15000) {
    for (int i = 0; i < 6; i++) if (digitalRead(KEYS[i]) == LOW) { Serial.printf("KEY%d (GPIO%d) pressed\n", i + 1, KEYS[i]); delay(250); }
    if (digitalRead(PIN_HOOK) == LOW) { Serial.println("GPIO22 (hook) pulled low"); delay(250); }
    if (digitalRead(PIN_KEY7) == LOW) { Serial.println("GPIO21 (key 7) pulled low"); delay(250); }
    if (Serial.available() && Serial.read() == 'x') break;
  }
  Serial.println("PASS if all 8 printed once each. KEY1 never prints = missing 10k pull-up on GPIO36.");
}

// ---------- 3 ----------
bool mountSd() {
  if (!SD_MMC.begin("/sdcard", true)) {            // true = 1-bit mode, leaves GPIO13 free for KEY2
    Serial.println("SD mount FAILED. Check: card inserted, DIP switches CMD + DATA3 ON, card formatted FAT32.");
    return false;
  }
  return true;
}
void stageSd() {
  Serial.println("Stage 3: SD card");
  if (!mountSd()) return;
  Serial.printf("Card size: %llu MB\n", SD_MMC.cardSize() / (1024 * 1024));
  File f = SD_MMC.open("/test.txt", FILE_WRITE); f.println("stranger phone sd ok"); f.close();
  f = SD_MMC.open("/test.txt"); Serial.print("Read back: "); while (f.available()) Serial.write(f.read()); f.close();
  File root = SD_MMC.open("/"); File e = root.openNextFile();
  while (e) { Serial.printf("  %s  %u bytes\n", e.name(), (unsigned)e.size()); e = root.openNextFile(); }
  Serial.println("PASS if the line read back and files listed.");
}

// ---------- 4 ----------
void stageTone() {
  Serial.println("Stage 4: 440 Hz tone for 3 s on the PHONE jack");
  auto cfg = kit.defaultConfig(TX_MODE);
  cfg.sd_active = false;
  cfg.sample_rate = 44100; cfg.channels = 2; cfg.bits_per_sample = 16;
  kit.begin(cfg);
  kit.setVolume(0.6);
  sine.begin(cfg, N_A4);
  StreamCopy copier(kit, sound);
  unsigned long t0 = millis();
  while (millis() - t0 < 3000) copier.copy();
  kit.end();
  Serial.println("PASS if you heard a clean tone. Buzz/hum = bad ground on the earpiece lead. Silence = check I2C to ES8388 (GPIO32/33).");
}

// ---------- 5 ----------
void stageRecord() {
  Serial.println("Stage 5: recording 5 s from the mic to /rec.wav ... speak now");
  if (!mountSd()) return;
  auto cfg = kit.defaultConfig(RX_MODE);
  cfg.sd_active = false;
  cfg.input_device = ADC_INPUT_LINE2;             // onboard mics / MIC1 pads on the v2.2 board
  cfg.sample_rate = 16000; cfg.channels = 1; cfg.bits_per_sample = 16;
  kit.begin(cfg);
  SD_MMC.remove("/rec.wav");
  File f = SD_MMC.open("/rec.wav", FILE_WRITE);
  WAVEncoder enc;
  EncodedAudioStream out(&f, &enc);
  out.begin(cfg);
  StreamCopy copier(out, kit);
  unsigned long t0 = millis();
  while (millis() - t0 < 5000) copier.copy();
  out.end(); f.close(); kit.end();
  Serial.printf("Saved /rec.wav. PASS if the file is > 150 kB. Now run 6.\n");
}

// ---------- 6 ----------
void stagePlay() {
  Serial.println("Stage 6: playing /rec.wav");
  if (!mountSd()) return;
  File f = SD_MMC.open("/rec.wav");
  if (!f) { Serial.println("No /rec.wav - run stage 5 first"); return; }
  auto cfg = kit.defaultConfig(TX_MODE);
  cfg.sd_active = false;
  cfg.sample_rate = 16000; cfg.channels = 1; cfg.bits_per_sample = 16;
  kit.begin(cfg);
  kit.setVolume(0.8);
  WAVDecoder dec;
  EncodedAudioStream in(&f, &dec);
  in.begin();
  StreamCopy copier(kit, in);
  while (copier.copy());
  f.close(); kit.end();
  Serial.println("PASS if you heard yourself clearly. Too quiet = raise setVolume / MAX9814 gain. Distorted = add the 10k/1k divider.");
}

// ---------- 7 ----------
void stageKeepAlive() {
  Serial.println("Stage 7: keep-alive. Wi-Fi radio on (no connection) + 47 ohm 1 W across 5V/GND.");
  Serial.println("Plug the board into the POWER BANK (not the laptop), leave overnight. PASS = still running in the morning.");
  WiFi.mode(WIFI_STA); WiFi.disconnect(false);     // radio on, not connected: ~+40-60 mA
}
