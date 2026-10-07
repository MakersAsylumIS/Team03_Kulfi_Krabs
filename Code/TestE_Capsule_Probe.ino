// TEST E — Bare electret capsule (fallback mic): level meter on GPIO36
// Connect: 10 kOhm from board 3V3 to capsule (+); capsule (+) also to the KEY1 pad (GPIO36); capsule (-) to GND. USB (UART).
//          (The capsule needs the 10 k from 3V3 to work at all - it is a powered part.)
// Pass:    swing clearly rises when you talk at it. Expect smaller numbers than the MAX9814: 150-400 while speaking is fine.

const int PIN_ADC = 36;                                 // GPIO36 = analog input, the KEY1 pad

void setup() {                                          // runs once
  Serial.begin(115200);                                 // open serial
  delay(300);                                           // settle
  analogReadResolution(12);                             // 0..4095 scale
  Serial.println("TEST E: electret capsule level meter. Silence, then talk.");  // instructions
}

void loop() {                                           // runs forever
  int mn = 4095;                                        // lowest sample this window
  int mx = 0;                                           // highest sample this window
  long sum = 0;                                         // for the average
  for (int i = 0; i < 400; i++) {                       // 400 samples ~ 20 ms
    int v = analogRead(PIN_ADC);                        // read the capsule voltage
    if (v < mn) mn = v;                                 // min
    if (v > mx) mx = v;                                 // max
    sum += v;                                           // total
    delayMicroseconds(50);                              // ~20 kHz
  }
  int avg = sum / 400;                                  // idle level depends on the capsule: anywhere 1000-2500 is normal
  int swing = mx - mn;                                  // peak-to-peak = loudness
  Serial.printf("avg %4d  swing %4d  ", avg, swing);    // numbers
  for (int i = 0; i < swing / 20; i++) {                // bar: one # per 20 counts (finer than test D because the signal is smaller)
    Serial.print('#');                                  // bar segment
  }
  Serial.println();                                     // newline
  delay(150);                                           // readable rate
}
