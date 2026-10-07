// TEST B — microSD card in the board: mount, report size, write a file, read it back, list the root
// Connect: USB (UART) + microSD (formatted FAT32) in the slot, DIP switches CMD and DATA3 ON.
// Pass:    "Read back: sd ok" appears and the file list shows test.txt.

#include "SD_MMC.h"                                      // ESP32's built-in SDMMC card driver

void setup() {                                           // runs once; whole test lives here
  Serial.begin(115200);                                  // open serial
  delay(300);                                            // settle
  Serial.println("TEST B: microSD card");                // banner

  if (!SD_MMC.begin("/sdcard", true)) {                  // mount at /sdcard in 1-bit mode (true) so GPIO13/KEY2 stays free
    Serial.println("Mount FAILED: card seated? DIP CMD+DATA3 ON? formatted FAT32?");  // most common causes
    return;                                              // stop here
  }
  Serial.printf("Card size: %llu MB\n", SD_MMC.cardSize() / (1024ULL * 1024ULL));  // print capacity in MB

  File f = SD_MMC.open("/test.txt", FILE_WRITE);         // create/overwrite a small test file
  if (!f) {                                              // if the open failed...
    Serial.println("Could not create /test.txt (card write-protected or corrupt)");  // ...say so
    return;                                              // and stop
  }
  f.println("sd ok");                                    // write one line
  f.close();                                             // close so it is flushed to the card

  f = SD_MMC.open("/test.txt");                          // reopen for reading
  Serial.print("Read back: ");                           // label
  while (f.available()) {                                // while there are bytes left...
    Serial.write(f.read());                              // ...echo each byte to the monitor
  }
  f.close();                                             // close the file

  Serial.println("Files in root:");                      // header for the listing
  File root = SD_MMC.open("/");                          // open the root directory
  File entry = root.openNextFile();                      // first entry
  while (entry) {                                        // while there are entries...
    Serial.printf("  %s  (%u bytes)\n", entry.name(), (unsigned)entry.size());  // name and size
    entry = root.openNextFile();                         // next entry
  }
  SD_MMC.end();                                          // unmount cleanly
  Serial.println("PASS if 'sd ok' was read back.");      // verdict
}

void loop() {                                            // nothing after setup
  delay(1000);                                           // idle
}
