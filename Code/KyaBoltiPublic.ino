/*
  KYA BOLTI PUBLIC — firmware for the ESP32 Audio Kit V2.2 (A541, ES8388 codec)
  ================================================================================
  Flow (matches flowchart Version 6):
    IDLE → handset lifted
         → "1) Pick a language"  (1 EN / 2 HI / 3 MR; 10 s, asked twice, default Hindi)
         → "2) This phone connects you to the people of Mumbai."
         → a stranger's message ("User Recording N") → "Single Beep"
         → "3) Press 4 or press 5"   (4 loops back for another message, as often as they like)
         → 5: "4) Please record your message after the beep." → "Single Beep" → RECORD 30 s → "Single Beep"
              → the take is played back → "5) Press 6 to save - Press 7 to redo"
              → 7: delete the take and record again (max 3 redos, then it is saved anyway)
              → 6 (or 10 s silence): save as <lang>/"User Recording <next number>.wav" → "6) Your message is saved"
              → "7) enjoy the rest of your ride" → "End Call Beep" → END (wait for hang-up, nothing else plays)
    no "User Recording" files yet → skip the message and the menu, go straight to recording.
    The language key (1/2/3) picks which messages are heard (that language's folder + the shared root) and
    where a new message is saved. The spoken prompts are one set for now.
    Hang-up at ANY point → stop everything, delete the scratch take, back to IDLE.

  Wiring (StrangerPhone_schematic_A541.png):
    keys 1..6  = onboard KEY1..KEY6   GPIO 36, 13, 19, 23, 18, 5   (to GND when pressed)
    key 7      = header IO21          (to GND when pressed; IO21 is also PA_ENABLE — we never drive it)
    hook       = header IO22          (switch to GND)
    mic        = MAX9814 → LINEIN jack (tip = OUT, sleeve = GND); VDD on header 3V3
    earpiece   = EARPHONES jack (tip + sleeve)
    SD card    = onboard slot, 1-bit mode. DIP: 1 ON, 2 OFF, 3 ON, 4 OFF, 5 OFF
    keep-alive = 47 Ω 1 W between header 3V3 and GND (power-bank use)

  SD card layout — the folder "ai prototype audios" copied AS IS to the root of the card:
    /ai prototype audios/1) Pick a language.mp3
    /ai prototype audios/2) This phone connects you to the people of Mumbai..mp3     (note the double dot — as named)
    /ai prototype audios/3) Press 4 or press 5.mp3
    /ai prototype audios/4) Please record your message after the beep..mp3          (double dot — as named)
    /ai prototype audios/5) Press 6 to save - Press 7 to redo.mp3
    /ai prototype audios/6) Your message is saved.mp3
    /ai prototype audios/7) enjoy the rest of your ride.mp3
    /ai prototype audios/Single Beep.mp3
    /ai prototype audios/End Call Beep.mp3
    /ai prototype audios/User Recording 1.mp3     messages in the ROOT of the folder play for EVERY language (your test ones)
    /ai prototype audios/User Recording 2.mp3
    /ai prototype audios/User Recording 3.mp3
    /ai prototype audios/en/User Recording 1.wav  messages recorded by English callers (key 1) — the phone writes here
    /ai prototype audios/hi/User Recording 1.wav  Hindi callers (key 2)
    /ai prototype audios/mr/User Recording 1.wav  Marathi callers (key 3)
    /tmp/take.wav                         scratch recording (created and deleted by the phone)
    /log.csv                              event log: millis,event,detail,lang
  Everything is MP3 or 16-bit PCM WAV; the firmware picks the decoder from the file extension.
  Messages: any file whose name starts with "User Recording" counts. A caller who chose Hindi hears the
  files in hi/ plus the ones in the root; a new message is saved into the caller's own language folder,
  numbered on from the highest number already there. In one call a message is never repeated; once a
  caller has heard all of them the list resets and they can be heard again.
  Run check_sd_files.py on your local copy of the folder — it verifies names and formats.

  Libraries (Library Manager, all by pschatzmann):
    arduino-audio-tools, arduino-audio-driver, arduino-libhelix (MP3 decoder)
  Board: ESP32 Dev Module · Partition: Huge APP · upload via the micro-USB marked UART.
*/

#include "AudioTools.h"                                   // audio framework: streams, copy loop, WAV codec
#include "AudioTools/AudioCodecs/CodecMP3Helix.h"         // MP3 decoder (needs the arduino-libhelix library)
#include "AudioTools/AudioLibs/AudioBoardStream.h"        // codec driver layer (ES8388 on the Audio Kit)
#include "SD_MMC.h"                                       // ESP32 built-in SD card driver (SDMMC bus)
#include <WiFi.h>                                         // only to keep the radio on for the power bank (keep-alive)

// =============================== SETTINGS YOU MAY CHANGE ===============================
const bool  HOOK_CLOSED_ON_CRADLE = true;   // true: switch contacts are CLOSED (pin LOW) when the handset rests on the cradle.
                                            // Flip to false if the phone behaves as "lifted" while it sits on the cradle.
const input_device_t MIC_INPUT = ADC_INPUT_LINE2;  // LINE2 = the LINEIN jack in this driver (LINE1 = onboard mics). If recordings are silent, try ADC_INPUT_LINE1.
const float VOLUME_PROMPTS   = 1.00;        // codec (hardware) earpiece volume for prompts (0.0 – 1.0, 1.0 = codec maximum)
const float VOLUME_MESSAGES  = 1.00;        // codec volume for strangers' messages and the playback of the user's own take
const float VOLUME_BOOST     = 1.5;         // extra DIGITAL gain applied to every sample before the codec (1.0 = none).
                                            // The audio files are now normalised to the same loudness (check_sd_files.py / ffmpeg),
                                            // so only a little boost is needed. Anything above ~2.0 clips and sounds harsh.
const float MIC_GAIN         = 0.70;        // codec input gain for recording (0.0 – 1.0); lower if recordings clip
const int   RECORD_SECONDS   = 30;          // message length: recording always runs the full 30 s, then Single Beep
const int   LANG_TIMEOUT_MS  = 10000;       // wait for 1/2/3 before repeating the question once
const int   MENU_TIMEOUT_MS  = 8000;        // wait for 4/5 before repeating the menu once
const int   REVIEW_TIMEOUT_MS= 10000;       // wait for 6/7; timeout = treat as 6 (save)
const int   MAX_REDOS        = 3;           // how many times 7 may be pressed; after that the take is saved without asking
const bool  PLAY_TAKE_BACK   = true;        // play the person's own recording back to them before "Press 6 to save - Press 7 to redo"
const bool  KEEP_WIFI_RADIO_ON = true;      // adds ~50 mA so the power bank never thinks the phone is idle
const char* DEFAULT_LANG     = "hi";        // used when nobody presses 1/2/3
// ========================================================================================

// ---- the recordings, named EXACTLY as the files in the folder "ai prototype audios" ----
const char* AUDIO_DIR   = "/ai prototype audios/";                                  // folder at the root of the SD card
const char* PR_LANG     = "1) Pick a language.mp3";                                 // "press 1 / 2 / 3"
const char* PR_INTRO    = "2) This phone connects you to the people of Mumbai..mp3";// the intro line (no choice)
const char* PR_MENU     = "3) Press 4 or press 5.mp3";                              // 4 = another message, 5 = record
const char* PR_REC      = "4) Please record your message after the beep..mp3";      // before recording
const char* PR_REVIEW   = "5) Press 6 to save - Press 7 to redo.mp3";               // after the take has played back
const char* PR_SAVED    = "6) Your message is saved.mp3";                           // after saving
const char* PR_RIDE     = "7) enjoy the rest of your ride.mp3";                     // the last spoken line
const char* SND_BEEP    = "Single Beep.mp3";                                        // after every played message and after the 30 s recording
const char* SND_ENDBEEP = "End Call Beep.mp3";                                      // the very last sound of the call
const char* MSG_PREFIX  = "User Recording ";                                        // strangers' messages: "User Recording 1.mp3", "User Recording 2.wav", ...

// ---- pins ----
const int PIN_KEY[8] = {0, 36, 13, 19, 23, 18, 5, 21};   // index 1..7 = phone keys 1..7 (index 0 unused)
const int PIN_HOOK   = 22;                               // hook switch on header IO22

// ---- codec pin map WITHOUT the PA (IO21), LED (IO22), keys and SD-SPI pins, so the driver never touches them ----
class KBPPins : public audio_driver::DriverDeviceInfo {   // our own pin description for the Audio Kit
 public:
  KBPPins() {                                             // runs once at start-up
    addI2C(PinFunction::CODEC, 32, 33);                   // I2C to the ES8388: SCL 32, SDA 33 (fixed by the board)
    addI2S(PinFunction::CODEC, 0, 27, 25, 26, 35);        // I2S: MCLK 0, BCLK 27, WS 25, DOUT 26, DIN 35 (fixed by the board)
  }                                                       // nothing else: IO21/IO22 stay free for key 7 and the hook
};
KBPPins kbpPins;                                          // the pin map object
audio_driver::AudioBoard kbpBoard(audio_driver::AudioDriverES8388, kbpPins);  // ES8388 driver + our pins
AudioBoardStream kit(kbpBoard);                           // the audio stream we play into and record from

// ---- digital volume boost: multiplies every 16-bit sample by VOLUME_BOOST, with hard limiting, then passes it to the codec ----
class BoostOutput : public AudioOutput {                  // sits between the decoder and the codec
 public:
  BoostOutput(AudioStream& out, float gain) : p_out(&out), gain(gain) {}
  void setAudioInfo(AudioInfo info) override {            // the decoder tells us the file's rate/channels...
    AudioOutput::setAudioInfo(info);
    p_out->setAudioInfo(info);                            // ...and we pass that on to the codec
  }
  size_t write(const uint8_t* data, size_t len) override {  // called with decoded PCM
    size_t done = 0;
    while (done < len) {                                  // work in small chunks so we never need a big buffer
      size_t n = min(len - done, sizeof(buf));
      memcpy(buf, data + done, n);                        // copy (the decoder's buffer is read-only to us)
      int16_t* smp = (int16_t*)buf;                       // our files are 16-bit, so 2 bytes per sample
      for (size_t i = 0; i < n / 2; i++) {
        int32_t v = (int32_t)(smp[i] * gain);             // scale
        if (v > 32767) v = 32767;                         // limit: never wrap around (that is what crackles)
        if (v < -32768) v = -32768;
        smp[i] = (int16_t)v;
      }
      p_out->write(buf, n);                               // on to the codec
      done += n;
    }
    return len;
  }
 private:
  AudioStream* p_out;
  float gain;
  uint8_t buf[512];
};

// ---- session state (reset on every hang-up) ----
String   lang = "";                                       // "en" / "hi" / "mr" once chosen
String   played[64];                                      // message files already played this session
int      playedCount = 0;                                 // how many entries in played[]

// ======================================================================================
//                                   SMALL HELPERS
// ======================================================================================

bool lifted() {                                           // true while the handset is OFF the cradle
  int v = digitalRead(PIN_HOOK);                          // LOW = switch closed, HIGH = switch open
  return HOOK_CLOSED_ON_CRADLE ? (v == HIGH) : (v == LOW);  // translate according to the setting above
}

bool keyDown(int k) {                                     // true while phone key k (1..7) is held
  return digitalRead(PIN_KEY[k]) == LOW;                  // every key shorts its pin to GND when pressed
}

void logEvent(const char* ev, const String& detail = "") {  // append one line to /log.csv on the SD card
  File f = SD_MMC.open("/log.csv", FILE_APPEND);          // open (or create) the log in append mode
  if (!f) return;                                         // no card? skip silently
  f.printf("%lu,%s,%s,%s\n", millis(), ev, detail.c_str(), lang.c_str());  // millis since boot, event, free text, language
  f.close();                                              // close so the line is really on the card
  Serial.printf("[%lu] %s %s\n", millis(), ev, detail.c_str());  // mirror to the serial monitor for debugging
}

String A(const char* name) {                              // full path of one of the operator recordings
  return String(AUDIO_DIR) + name;                        // e.g. "/ai prototype audios/Single Beep.mp3"
}

// Wait up to `timeoutMs` for one of the keys in `allowed` (a string like "45").
// Returns the key number, 0 on timeout, or -1 if the handset was hung up.
int waitKey(const char* allowed, int timeoutMs) {
  unsigned long t0 = millis();                            // remember when we started waiting
  while (millis() - t0 < (unsigned long)timeoutMs) {      // until the timeout...
    if (!lifted()) return -1;                             // hung up → tell the caller to abort
    for (int k = 1; k <= 7; k++) {                        // scan the seven keys
      if (strchr(allowed, '0' + k) && keyDown(k)) {       // this key is allowed AND pressed
        delay(30);                                        // debounce
        if (!keyDown(k)) continue;                        // it was a glitch, keep scanning
        while (keyDown(k)) { if (!lifted()) return -1; delay(10); }  // wait for release so one press = one event
        return k;                                         // report the key
      }
    }
    delay(5);                                             // small pause so the loop isn't a tight spin
  }
  return 0;                                               // nothing pressed in time
}

// ======================================================================================
//                                   AUDIO: PLAY
// ======================================================================================

// Play one file (MP3 or 16-bit PCM WAV) into the earpiece. Returns false if the handset was hung up mid-way.
bool play(const String& path, float volume = VOLUME_PROMPTS) {
  File f = SD_MMC.open(path);                             // open the file on the card
  if (!f) {                                               // missing file?
    Serial.printf("MISSING FILE: %s\n", path.c_str());    // say so in the monitor...
    logEvent("missing", path);                            // ...and in the log, then carry on without it
    return lifted();                                      // treat as "played" unless hung up
  }
  String lower = path; lower.toLowerCase();               // decide the decoder from the extension
  bool isMp3 = lower.endsWith(".mp3");
  Serial.printf("playing %s (%lu bytes, %s)\n", path.c_str(), (unsigned long)f.size(), isMp3 ? "mp3" : "wav");

  auto cfg = kit.defaultConfig(TX_MODE);                  // playback configuration
  cfg.sd_active = false;                                  // we handle the SD card ourselves
  cfg.sample_rate = 16000;                                // starting format — the decoder tells the codec the real one
  cfg.channels = 1;                                       // (our own recordings are 16 kHz mono 16-bit)
  cfg.bits_per_sample = 16;
  kit.begin(cfg);                                         // start the codec for playback
  kit.setVolume(volume);                                  // set the earpiece level

  WAVDecoder wavDec;                                      // decoder for .wav
  MP3DecoderHelix mp3Dec;                                 // decoder for .mp3
  AudioDecoder* dec = isMp3 ? (AudioDecoder*)&mp3Dec : (AudioDecoder*)&wavDec;  // pick one
  BoostOutput boost(kit, VOLUME_BOOST);                   // digital gain stage in front of the codec
  EncodedAudioStream out(&boost, dec);                    // decoded audio → boost → codec; the codec is told the file's rate/channels
  out.begin();                                            // start the decoder
  StreamCopy copier(out, f);                              // copies file bytes into the decoder
  bool ok = true;                                         // assume we finish normally
  while (copier.copy()) {                                 // copy chunk by chunk until the file ends
    if (!lifted()) { ok = false; break; }                 // hang-up → stop immediately
  }
  out.end();                                              // flush the decoder
  f.close();                                              // close the file
  kit.end();                                              // release the codec (next op may be a record)
  Serial.printf("  done%s\n", ok ? "" : " (hung up)");
  return ok;                                              // false only on hang-up
}

// Convenience: play one of the operator recordings by its file name.
bool say(const char* name) { return play(A(name)); }

// ======================================================================================
//                                   AUDIO: RECORD
// ======================================================================================

// Record from the mic into `path` for `seconds`. Returns false if hung up (file is then deleted).
bool record(const String& path, int seconds) {
  SD_MMC.remove(path);                                    // start from a clean file
  File f = SD_MMC.open(path, FILE_WRITE);                 // create the scratch file
  if (!f) { logEvent("error", "cannot create " + path); return false; }  // card problem
  auto cfg = kit.defaultConfig(RX_MODE);                  // recording configuration
  cfg.sd_active = false;                                  // we handle the SD card ourselves
  cfg.input_device = MIC_INPUT;                           // LINEIN jack (see setting at the top)
  cfg.sample_rate = 16000;                                // 16 kHz mono 16-bit
  cfg.channels = 1;
  cfg.bits_per_sample = 16;
  kit.begin(cfg);                                         // start the codec for recording
  kit.setInputVolume(MIC_GAIN);                           // input gain
  WAVEncoder enc;                                         // writes a proper WAV header + samples
  EncodedAudioStream out(&f, &enc);                       // file wrapped in the encoder
  out.begin(cfg);                                         // header gets the right format
  StreamCopy copier(out, kit);                            // copies mic samples into the file
  bool ok = true;                                         // assume normal end
  unsigned long t0 = millis();                            // start time
  while (millis() - t0 < (unsigned long)seconds * 1000UL) {  // until the time limit...
    copier.copy();                                        // ...keep copying audio
    if (!lifted()) { ok = false; break; }                 // hang-up → stop
  }
  out.end();                                              // finalise the WAV header with the real length
  f.close();                                              // close the file
  kit.end();                                              // release the codec
  if (!ok) SD_MMC.remove(path);                           // hung up: nothing is kept
  return ok;
}

// ======================================================================================
//                                   FILE PICKING
// ======================================================================================

// True if `name` is a stranger's message: starts with "User Recording " and ends in .mp3 or .wav
bool isMessageFile(const String& name) {
  String lower = name; lower.toLowerCase();               // case-insensitive extension check
  bool ext = lower.endsWith(".mp3") || lower.endsWith(".wav");
  return name.startsWith(MSG_PREFIX) && ext && !name.startsWith(".");
}

// Bare file name of one directory entry (the SD library sometimes returns the full path)
String bareName(File& e) {
  String name = e.name();
  int slash = name.lastIndexOf('/');
  return slash >= 0 ? name.substring(slash + 1) : name;
}

// Folder for the current language's messages, e.g. "/ai prototype audios/hi/"
String langDir() { return String(AUDIO_DIR) + lang + "/"; }

// Open a folder. The SD driver refuses a path that ends in "/", so strip it first.
File openDir(String path) {
  while (path.length() > 1 && path.endsWith("/")) path.remove(path.length() - 1);
  File d = SD_MMC.open(path);
  if (!d || !d.isDirectory()) Serial.printf("folder not found: %s\n", path.c_str());
  return d;
}

// Collect message files from one folder into `out` (as "<sub>name", sub = "" for the root or "hi/" etc.).
int collectMessages(const String& dir, const String& sub, String* out, int n, int maxN) {
  File d = openDir(dir);                                  // open the folder
  if (!d) return n;                                       // folder missing → nothing added
  File e = d.openNextFile();                              // first entry
  while (e && n < maxN) {                                 // walk the folder
    String name = bareName(e);
    if (!e.isDirectory() && isMessageFile(name)) out[n++] = sub + name;  // keep it
    e = d.openNextFile();                                 // next entry
  }
  return n;
}

// Pick a random message for the current language, not in `exclude`. Returns "" if none.
// Candidates = files in the language folder (hi/, en/, mr/) PLUS files in the root of the audio folder (for every language).
static String all[64];                                    // scratch lists kept OUT of the (small) task stack
static String candidates[64];
String pickRandomMessage(String* exclude, int excludeCount) {
  int n = 0;
  n = collectMessages(langDir(), lang + "/", all, n, 64);           // this language's messages
  n = collectMessages(String(AUDIO_DIR), "", all, n, 64);           // messages for everyone (root of the folder)
  Serial.printf("messages found: %d (lang=%s)\n", n, lang.c_str());
  int c = 0;
  for (int i = 0; i < n; i++) {                           // drop the ones already played this call
    bool excluded = false;
    for (int j = 0; j < excludeCount; j++) if (exclude[j] == all[i]) excluded = true;
    if (!excluded) candidates[c++] = all[i];
  }
  if (c == 0) return "";                                  // nothing usable
  return candidates[esp_random() % c];                    // true random pick
}

// A random stranger's message in the caller's language, not played yet this session.
String randomMessage() {
  String name = pickRandomMessage(played, playedCount);   // exclude already played
  if (name.length() == 0 && playedCount > 0) {            // everything has been heard once this session?
    logEvent("played_all");
    playedCount = 0;                                      // ...start over rather than go silent
    name = pickRandomMessage(played, 0);
  }
  if (name.length() && playedCount < 64) played[playedCount++] = name;  // remember it
  return name.length() ? String(AUDIO_DIR) + name : "";   // full path or ""
}

// Highest N among the "User Recording N.*" files in `dir` (0 if none).
int highestMessageNumber(const String& dir) {
  int best = 0;
  File d = openDir(dir);
  if (!d) return 0;
  File e = d.openNextFile();
  while (e) {
    String name = bareName(e);
    if (!e.isDirectory() && isMessageFile(name)) {
      int n = name.substring(strlen(MSG_PREFIX)).toInt(); // the number after "User Recording "
      if (n > best) best = n;
    }
    e = d.openNextFile();
  }
  return best;
}

// Move the scratch take into the language folder as "User Recording <next>.wav". Returns the new path.
String saveTake() {
  SD_MMC.mkdir(String(AUDIO_DIR) + lang);                 // make sure hi/ en/ mr/ exists (no trailing slash)
  int next = highestMessageNumber(langDir()) + 1;         // numbering continues per language
  String dest = langDir() + MSG_PREFIX + next + ".wav";   // e.g. "/ai prototype audios/hi/User Recording 4.wav"
  SD_MMC.rename("/tmp/take.wav", dest);                   // move (rename) the scratch file into place
  return dest;
}

// ======================================================================================
//                                   THE CALL FLOW
// ======================================================================================

// Wait silently with the handset lifted until it is put down (used after End Call Beep).
void waitForHangup() {
  while (lifted()) delay(50);                             // nothing plays, keys are ignored
}

// Step: ask for the language. Returns false on hang-up.
bool stepLanguage() {
  for (int attempt = 0; attempt < 2; attempt++) {         // ask at most twice
    if (!say(PR_LANG)) return false;                      // "1) Pick a language"
    int k = waitKey("123", LANG_TIMEOUT_MS);              // wait for 1, 2 or 3
    if (k == -1) return false;                            // hung up
    if (k == 1) { lang = "en"; return true; }             // English
    if (k == 2) { lang = "hi"; return true; }             // Hindi
    if (k == 3) { lang = "mr"; return true; }             // Marathi
  }
  lang = DEFAULT_LANG;                                    // nobody pressed anything twice → default
  logEvent("lang_default", lang);
  return true;
}

// Step: play one stranger's message + Single Beep. Returns 1 = played, 0 = no messages yet, -1 = hung up.
int stepMessage() {
  String m = randomMessage();                             // pick one
  if (m.length() == 0) {                                  // no "User Recording" files on the card yet
    logEvent("none_yet");
    return 0;                                             // caller jumps straight to recording
  }
  logEvent("play", m);
  if (!play(m, VOLUME_MESSAGES)) return -1;               // the message
  if (!say(SND_BEEP)) return -1;                          // "Single Beep" — end of the message
  return 1;
}

// Step: the record branch (5 was pressed, or there were no messages yet). Returns when the call is over.
void stepRecord() {
  SD_MMC.mkdir("/tmp");                                   // scratch folder
  int redos = 0;                                          // how many times 7 has been pressed
  while (true) {                                          // loop until saved or hung up
    if (!say(PR_REC)) return;                             // "4) Please record your message after the beep."
    if (!say(SND_BEEP)) return;                           // "Single Beep" — start talking
    logEvent("record_start", String(redos));
    if (!record("/tmp/take.wav", RECORD_SECONDS)) return; // 30 s of recording (deleted automatically on hang-up)
    logEvent("record_end");
    if (!say(SND_BEEP)) { SD_MMC.remove("/tmp/take.wav"); return; }  // "Single Beep" — the 30 s are over
    if (redos >= MAX_REDOS) break;                        // no more redos allowed → save without asking
    if (PLAY_TAKE_BACK) {                                 // let them hear what they recorded
      if (!play("/tmp/take.wav", VOLUME_MESSAGES)) { SD_MMC.remove("/tmp/take.wav"); return; }
    }
    if (!say(PR_REVIEW)) { SD_MMC.remove("/tmp/take.wav"); return; }  // "5) Press 6 to save - Press 7 to redo"
    int k = waitKey("67", REVIEW_TIMEOUT_MS);             // wait for 6 or 7
    if (k == -1) { SD_MMC.remove("/tmp/take.wav"); return; }  // hung up → discard
    if (k == 7) {                                         // redo
      redos++;
      SD_MMC.remove("/tmp/take.wav");                     // delete the previous take — nothing from it survives
      logEvent("redo", String(redos));
      continue;                                           // back to the top: record again
    }
    break;                                                // 6 pressed, or timeout → save
  }
  String saved = saveTake();                              // becomes "User Recording <next>.wav"
  logEvent("save", saved);
  if (!say(PR_SAVED)) return;                             // "6) Your message is saved"
  if (!say(PR_RIDE)) return;                              // "7) enjoy the rest of your ride"
  say(SND_ENDBEEP);                                       // "End Call Beep" — the last sound
  logEvent("end");
  waitForHangup();                                        // END: nothing more happens until the handset is put down
}

// One complete call, from lift to hang-up.
void session() {
  lang = ""; playedCount = 0;                             // fresh session state
  logEvent("lift");
  if (!stepLanguage()) return;                            // 1/2/3
  logEvent("lang", lang);

  if (!say(PR_INTRO)) return;                             // "2) This phone connects you to the people of Mumbai." (no choice)

  int r = stepMessage();                                  // first stranger's message + Single Beep
  if (r == -1) return;                                    // hung up
  if (r == 0) { stepRecord(); return; }                   // no messages yet → straight to recording

  while (true) {                                          // the 4 / 5 loop
    int k = 0;
    for (int attempt = 0; attempt < 2 && k == 0; attempt++) {  // ask at most twice
      if (!say(PR_MENU)) return;                          // "3) Press 4 or press 5"
      k = waitKey("45", MENU_TIMEOUT_MS);                 // wait for 4 or 5
      if (k == -1) return;                                // hung up
    }
    if (k == 0) {                                         // silent twice → end the call
      logEvent("menu_timeout");
      say(SND_ENDBEEP);                                   // "End Call Beep"
      waitForHangup();                                    // and wait for the cradle
      return;
    }
    if (k == 4) {                                         // another message
      logEvent("key4");
      r = stepMessage();                                  // play one + Single Beep
      if (r == -1) return;                                // hung up
      if (r == 0) { stepRecord(); return; }               // ran out of messages → record
      continue;                                           // back to the menu
    }
    if (k == 5) {                                         // record
      logEvent("key5");
      stepRecord();                                       // the whole record branch, ends with End Call Beep
      return;
    }
  }
}

// ======================================================================================
//                                   SETUP / LOOP
// ======================================================================================

void setup() {
  Serial.begin(115200);                                   // debug output
  delay(300);
  Serial.println("\nKYA BOLTI PUBLIC starting");

  for (int k = 1; k <= 7; k++) pinMode(PIN_KEY[k], INPUT_PULLUP);  // keys 1..7 as inputs with pull-ups (GPIO36 relies on the board's own resistor)
  pinMode(PIN_HOOK, INPUT_PULLUP);                        // hook switch input

  if (!SD_MMC.begin("/sdcard", true)) {                   // mount the card in 1-bit mode (keeps IO13 free for KEY2)
    Serial.println("SD card not found - check card, DIP switches 1 ON / 3 ON, FAT32");
    while (true) { delay(1000); }                         // nothing works without the card: stop here
  }
  SD_MMC.mkdir("/tmp");                                   // scratch folder
  SD_MMC.remove("/tmp/take.wav");                         // a leftover take from a power cut is never kept

  // check that every operator recording is on the card, so a typo shows up at boot and not mid-call
  const char* needed[] = {PR_LANG, PR_INTRO, PR_MENU, PR_REC, PR_REVIEW, PR_SAVED, PR_RIDE, SND_BEEP, SND_ENDBEEP};
  for (auto n : needed) {
    if (!SD_MMC.exists(A(n))) Serial.printf("WARNING missing on SD card: %s\n", A(n).c_str());
  }

  if (KEEP_WIFI_RADIO_ON) { WiFi.mode(WIFI_STA); WiFi.disconnect(false); }  // radio on, not connected: power-bank keep-alive

  const char* langs[3] = {"en", "hi", "mr"};              // language folders for saved messages
  for (int i = 0; i < 3; i++) SD_MMC.mkdir(String(AUDIO_DIR) + langs[i]);

  // list what the phone can see, so a naming problem is obvious before the first call
  Serial.println("Files in the audio folder:");
  File d = openDir(String(AUDIO_DIR));
  if (d) { File e = d.openNextFile(); while (e) { Serial.printf("  %s%s%s\n", e.isDirectory() ? "[dir] " : "", bareName(e).c_str(), isMessageFile(bareName(e)) ? "   <- stranger's message" : ""); e = d.openNextFile(); } }
  int found = collectMessages(String(AUDIO_DIR), "", all, 0, 64);
  Serial.printf("Messages for every language: %d\n", found);
  if (found == 0) Serial.println("NOTE: no 'User Recording N' files in the root of the folder — callers go straight to recording until some exist");
  logEvent("boot", String("en=") + highestMessageNumber(String(AUDIO_DIR) + "en/") +
                   " hi=" + highestMessageNumber(String(AUDIO_DIR) + "hi/") +
                   " mr=" + highestMessageNumber(String(AUDIO_DIR) + "mr/"));
  Serial.println("Ready. Lift the handset.");
}

void loop() {
  if (lifted()) {                                         // handset picked up
    session();                                            // run one complete call
    SD_MMC.remove("/tmp/take.wav");                       // make sure no scratch recording survives
    kit.end();                                            // make sure the codec is released
    while (lifted()) delay(50);                           // if still off the cradle, wait until it is put down
    logEvent("hangup");
    delay(300);                                           // settle before the next lift can start
  }
  delay(20);                                              // idle poll
}
