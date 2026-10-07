/*
  Stranger Phone — per-component test sketch for the ESP32-A1S Audio Kit v2.2 (ES8388)
  -------------------------------------------------------------------------------------
  Each test needs ONLY the part under test connected. Type the number in the Serial Monitor (115200).

    1  BOARD  : serial + LED D4 blink                      (nothing connected except USB)
    2  BOARD  : onboard KEY1..KEY6 read                    (nothing connected; KEY1 needs the 10k pull-up)
    3  BOARD  : 440 Hz tone on the PHONE jack              (any headphones)
    4  BOARD  : 5 s record from ONBOARD mics -> /rec.wav   (SD card inserted) then plays it back
    5  SD     : mount, size, write/read, list files
    6  MIC    : MAX9814 / electret level meter on GPIO36   (3 wires: VCC, GND, OUT->IO36)
    7  SWITCH : continuity on header pins IO21 / IO22      (any button / hook switch across pin and GND)
    8  POWER  : keep-alive mode for the power-bank overnight test
    9  SPEAKER: 1 kHz tone, 10 s, for the earpiece / 8 ohm speaker on the PHONE jack

  Libraries: arduino-audio-tools + arduino-audio-driver (pschatzmann), via Library Manager.
  Board: ESP32 Dev Module · Partition: Huge APP · port = micro-USB marked UART.
*/

#include "AudioTools.h"
#include "AudioTools/AudioLibs/AudioBoardStream.h"
#include "SD_MMC.h"
#include <WiFi.h>

const int KEYS[6]  = {36, 13, 19, 23, 18, 5};
const int PIN_LED  = 22;
const int PIN_H21  = 21;     // header IO21 (= PA_ENABLE: input only, never drive it)
const int PIN_H22  = 22;     // header IO22 (shares LED D4)
const int PIN_ADC  = 36;     // KEY1 / ADC1_CH0 - used as the mic level probe in test 6

AudioBoardStream kit(AudioKitEs8388V1);
SineWaveGenerator<int16_t> sine(12000);
GeneratedSoundStream<int16_t> sound(sine);

void setup() {
  Serial.begin(115200); delay(400);
  for (int i = 0; i < 6; i++) pinMode(KEYS[i], INPUT_PULLUP);   // GPIO36: external 10k to 3V3 needed
  pinMode(PIN_H21, INPUT_PULLUP);
  pinMode(PIN_H22, INPUT_PULLUP);
  Serial.println("\n=== Stranger Phone component tests ===");
  menu();
}
void menu() { Serial.println("1 LED 2 Keys 3 Tone 4 OnboardMicRec 5 SD 6 MicProbe 7 SwitchProbe 8 KeepAlive 9 SpeakerTone"); }

void loop() {
  if (!Serial.available()) return;
  switch (Serial.read()) {
    case '1': tLed(); break;   case '2': tKeys(); break;  case '3': tTone(440, 3000); break;
    case '4': tRecPlay(); break; case '5': tSd(); break; case '6': tMicProbe(); break;
    case '7': tSwitchProbe(); break; case '8': tKeepAlive(); break; case '9': tTone(1000, 10000); break;
    default: return;
  }
  menu();
}

// ---- 1
void tLed() {
  pinMode(PIN_LED, OUTPUT);
  for (int i = 0; i < 5; i++) { digitalWrite(PIN_LED, LOW); delay(200); digitalWrite(PIN_LED, HIGH); delay(200); }
  pinMode(PIN_H22, INPUT_PULLUP);
  Serial.println("PASS if LED D4 blinked 5x.");
}
// ---- 2
void tKeys() {
  Serial.println("Press each onboard key within 15 s ('x' to stop):");
  unsigned long t0 = millis();
  while (millis() - t0 < 15000) {
    for (int i = 0; i < 6; i++) if (digitalRead(KEYS[i]) == LOW) { Serial.printf("KEY%d GPIO%d\n", i + 1, KEYS[i]); delay(250); }
    if (Serial.available() && Serial.read() == 'x') break;
  }
  Serial.println("PASS if all six printed. KEY1 silent -> 10k pull-up on GPIO36 missing.");
}
// ---- 3 / 9
void tTone(int hz, int ms) {
  Serial.printf("%d Hz for %d ms on PHONE jack\n", hz, ms);
  auto cfg = kit.defaultConfig(TX_MODE); cfg.sd_active = false;
  cfg.sample_rate = 44100; cfg.channels = 2; cfg.bits_per_sample = 16;
  kit.begin(cfg); kit.setVolume(0.6); sine.begin(cfg, hz);
  StreamCopy c(kit, sound);
  unsigned long t0 = millis(); while (millis() - t0 < (unsigned)ms) c.copy();
  kit.end();
  Serial.println("PASS if clean tone. Silence -> codec/I2C fault (use spare board). Buzz -> earpiece ground not on sleeve.");
}
// ---- 5
bool mountSd() {
  if (!SD_MMC.begin("/sdcard", true)) { Serial.println("SD mount FAILED: card seated? DIP CMD+DATA3 ON? FAT32?"); return false; }
  return true;
}
void tSd() {
  if (!mountSd()) return;
  Serial.printf("Card: %llu MB\n", SD_MMC.cardSize() / (1024 * 1024));
  File f = SD_MMC.open("/test.txt", FILE_WRITE); f.println("sd ok"); f.close();
  f = SD_MMC.open("/test.txt"); Serial.print("Read back: "); while (f.available()) Serial.write(f.read()); f.close();
  File root = SD_MMC.open("/"); File e = root.openNextFile();
  while (e) { Serial.printf("  %s %u B\n", e.name(), (unsigned)e.size()); e = root.openNextFile(); }
  SD_MMC.end();
  Serial.println("PASS if 'sd ok' read back.");
}
// ---- 4
void tRecPlay() {
  if (!mountSd()) return;
  Serial.println("Recording 5 s from ONBOARD mics - talk at the board");
  auto cfg = kit.defaultConfig(RX_MODE); cfg.sd_active = false; cfg.input_device = ADC_INPUT_LINE2;
  cfg.sample_rate = 16000; cfg.channels = 1; cfg.bits_per_sample = 16;
  kit.begin(cfg);
  SD_MMC.remove("/rec.wav");
  File f = SD_MMC.open("/rec.wav", FILE_WRITE);
  WAVEncoder enc; EncodedAudioStream out(&f, &enc); out.begin(cfg);
  StreamCopy c(out, kit);
  unsigned long t0 = millis(); while (millis() - t0 < 5000) c.copy();
  out.end(); f.close(); kit.end();
  Serial.println("Playing it back...");
  f = SD_MMC.open("/rec.wav");
  auto cfg2 = kit.defaultConfig(TX_MODE); cfg2.sd_active = false;
  cfg2.sample_rate = 16000; cfg2.channels = 1; cfg2.bits_per_sample = 16;
  kit.begin(cfg2); kit.setVolume(0.8);
  WAVDecoder dec; EncodedAudioStream in(&f, &dec); in.begin();
  StreamCopy p(kit, in); while (p.copy());
  f.close(); kit.end(); SD_MMC.end();
  Serial.println("PASS if you heard yourself. This proves codec record + playback with no external parts.");
}
// ---- 6
void tMicProbe() {
  Serial.println("Mic probe on GPIO36 for 15 s. Idle ~1800-2200 (MAX9814) / ~1000-2500 (capsule+10k). Talk: swing should grow.");
  analogReadResolution(12);
  unsigned long t0 = millis();
  while (millis() - t0 < 15000) {
    int mn = 4095, mx = 0; long sum = 0;
    for (int i = 0; i < 400; i++) { int v = analogRead(PIN_ADC); mn = min(mn, v); mx = max(mx, v); sum += v; delayMicroseconds(50); }
    int avg = sum / 400, swing = mx - mn;
    Serial.printf("avg %4d  swing %4d  ", avg, swing);
    for (int i = 0; i < swing / 40; i++) Serial.print('#');
    Serial.println();
    delay(150);
  }
  Serial.println("PASS: swing < 60 in silence and > 400 when you speak 5 cm away. Stuck at 0 or 4095 -> wiring / no power.");
}
// ---- 7
void tSwitchProbe() {
  Serial.println("Switch probe 15 s: press the button / move the hook lever wired to IO21 or IO22.");
  unsigned long t0 = millis(); int last21 = -1, last22 = -1;
  while (millis() - t0 < 15000) {
    int a = digitalRead(PIN_H21), b = digitalRead(PIN_H22);
    if (a != last21) { Serial.printf("IO21: %s\n", a ? "OPEN" : "CLOSED"); last21 = a; }
    if (b != last22) { Serial.printf("IO22: %s\n", b ? "OPEN" : "CLOSED"); last22 = b; }
    delay(20);
  }
  Serial.println("PASS if it reports CLOSED/OPEN cleanly with no chatter.");
}
// ---- 8
void tKeepAlive() {
  WiFi.mode(WIFI_STA); WiFi.disconnect(false);
  Serial.println("Keep-alive ON (radio). Add 47 ohm 1 W across 5V/GND, move to the power bank, leave overnight. PASS = still on in the morning.");
}
