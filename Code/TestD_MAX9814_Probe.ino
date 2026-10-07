// TEST D — MAX9814 microphone module: level meter on the ESP32's analog pin GPIO36
// Connect: MAX9814 VCC -> board 3V3, GND -> board GND, OUT -> the KEY1 pad (GPIO36). USB (UART). Nothing else.
//          Do NOT have the 10 kOhm pull-up on GPIO36 fitted during this test.
// Pass:    "swing" stays below ~60 in silence and rises above ~400 when you speak 5 cm away.

const int PIN_ADC = 36;                                 // GPIO36 = ADC1 channel 0 = the KEY1 pad; input-only analog pin

void setup() {                                          // runs once
  Serial.begin(115200);                                 // open serial
  delay(300);                                           // settle
  analogReadResolution(12);                             // 12-bit readings: 0 .. 4095 for 0 .. ~3.3 V
  Serial.println("TEST D: MAX9814 level meter. Stay silent, then talk, then snap fingers.");  // instructions
}

void loop() {                                           // runs forever, one line per ~0.2 s
  int mn = 4095;                                        // will hold the lowest reading in this window
  int mx = 0;                                           // will hold the highest reading in this window
  long sum = 0;                                         // running total for the average
  for (int i = 0; i < 400; i++) {                       // take 400 quick samples (~20 ms)
    int v = analogRead(PIN_ADC);                        // read the mic output voltage as 0..4095
    if (v < mn) mn = v;                                 // track minimum
    if (v > mx) mx = v;                                 // track maximum
    sum += v;                                           // accumulate for average
    delayMicroseconds(50);                              // ~20 kHz sampling - fast enough to catch speech peaks
  }
  int avg = sum / 400;                                  // average level (~1500-2200 for the MAX9814's 1.25 V idle bias)
  int swing = mx - mn;                                  // peak-to-peak movement = how loud the sound was
  Serial.printf("avg %4d  swing %4d  ", avg, swing);    // print the numbers
  for (int i = 0; i < swing / 40; i++) {                // draw a bar: one # per 40 counts of swing
    Serial.print('#');                                  // bar segment
  }
  Serial.println();                                     // end the line
  delay(150);                                           // short pause so the monitor is readable
}
