/*
  TEST K — KEY6 routed OUT to the breadboard
  ==========================================
  Checks that the wire soldered to KEY6 reaches GPIO5 and that an external switch
  (wire ↔ GND) is seen exactly like the onboard button.

  Wiring:  soldered KEY6 wire → one side of your keypad switch (or just a loose jumper)
           other side of the switch → header GND
  Nothing else. No resistor: the board's pull-up on KEY6 does the job.

  Serial Monitor, 115200. On boot it prints the resting state (must be HIGH = released).
  Every change is printed with a timestamp, plus a count of presses.
  Touch the wire to GND by hand if you have no switch yet — that counts as a press.
*/

const int PIN_KEY6 = 5;                                   // KEY6 on the Audio Kit = GPIO5

int  lastState  = HIGH;                                   // what we last saw (HIGH = released, LOW = pressed)
int  presses    = 0;                                      // how many presses so far
unsigned long lastChange = 0;                             // millis() of the last accepted change

void setup() {
  Serial.begin(115200);                                   // open the serial monitor link
  delay(300);
  pinMode(PIN_KEY6, INPUT_PULLUP);                        // input with the ESP32's internal pull-up (board has one too)
  delay(20);                                              // let the line settle
  lastState = digitalRead(PIN_KEY6);                      // read the resting state once
  Serial.println("\nTEST K — KEY6 (GPIO5) external switch");
  Serial.printf("Resting state: %s\n", lastState == HIGH ? "HIGH = released (correct)" : "LOW = PRESSED ?! wire is shorted to GND or on the wrong leg");
  Serial.println("Press the external switch (or touch the wire to GND)...");
}

void loop() {
  int now = digitalRead(PIN_KEY6);                        // read the pin
  if (now != lastState && millis() - lastChange > 30) {   // changed, and not within the 30 ms debounce window
    lastChange = millis();                                // remember when
    lastState = now;                                      // remember what
    if (now == LOW) {                                     // went LOW = pressed
      presses++;
      Serial.printf("[%lu ms] PRESSED   (press #%d)\n", lastChange, presses);
    } else {                                              // went HIGH = released
      Serial.printf("[%lu ms] released\n", lastChange);
    }
  }
  delay(2);                                               // tiny pause, keeps the loop polite
}
