// TEST A2 — Read the six onboard buttons KEY1..KEY6
// Connect: only USB (UART). Nothing else.
// Pass:    pressing each button prints its name once. KEY1 stays silent unless a 10 kOhm
//          resistor is held from the KEY1 pad to 3V3 (GPIO36 has no internal pull-up) - expected.

const int KEYS[6] = {36, 13, 19, 23, 18, 5};        // GPIO numbers of KEY1..KEY6 on the Audio Kit
bool wasDown[6]   = {false, false, false, false, false, false};  // remembers each key's last state so we print once per press

void setup() {                                       // runs once
  Serial.begin(115200);                              // open serial at 115200 baud
  delay(300);                                        // let the port settle
  for (int i = 0; i < 6; i++) {                      // for each of the six keys...
    pinMode(KEYS[i], INPUT_PULLUP);                  // ...make it an input with the internal pull-up (reads HIGH until pressed to GND)
  }
  Serial.println("TEST A2: press each onboard key");  // instructions on the monitor
}

void loop() {                                        // runs forever
  for (int i = 0; i < 6; i++) {                      // scan all six keys
    bool down = (digitalRead(KEYS[i]) == LOW);       // LOW means the button is shorting the pin to GND = pressed
    if (down && !wasDown[i]) {                       // only on the moment it goes from released to pressed
      Serial.printf("KEY%d (GPIO%d) pressed\n", i + 1, KEYS[i]);  // print which key and which GPIO
    }
    wasDown[i] = down;                               // store state for the next scan
  }
  delay(20);                                         // 20 ms between scans = simple debounce
}
