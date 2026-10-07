// TEST G — Tactile button / micro switch / phone hook switch: read it on header pin IO22 (and IO21 if you like)
// Connect: one side of the switch to header IO22, the other side to GND. USB (UART). Nothing else.
//          IO21 is also watched so a second switch can be checked at the same time. IO21 is never driven as an output
//          (it is the board's PA_ENABLE line).
// Pass:    "IO22: CLOSED" when pressed, "IO22: OPEN" when released, printed exactly once per action, no chatter.

const int PIN_22 = 22;                                   // header IO22 (also lights LED D4 when pulled low - free visual confirmation)
const int PIN_21 = 21;                                   // header IO21 (= PA_ENABLE; input only)
int last22 = -1;                                         // previous reading of IO22 (-1 = not read yet)
int last21 = -1;                                         // previous reading of IO21

void setup() {                                           // runs once
  Serial.begin(115200);                                  // open serial
  delay(300);                                            // settle
  pinMode(PIN_22, INPUT_PULLUP);                         // input with internal pull-up: reads HIGH (open) until shorted to GND
  pinMode(PIN_21, INPUT_PULLUP);                         // same for IO21
  Serial.println("TEST G: press / release the switch on IO22 (or IO21)");  // instructions
}

void loop() {                                            // runs forever
  int v22 = digitalRead(PIN_22);                         // read IO22: HIGH = open, LOW = closed
  if (v22 != last22) {                                   // only act when the state changes
    Serial.printf("IO22: %s\n", v22 ? "OPEN" : "CLOSED");  // report the new state
    last22 = v22;                                        // remember it
  }
  int v21 = digitalRead(PIN_21);                         // read IO21
  if (v21 != last21) {                                   // on change...
    Serial.printf("IO21: %s\n", v21 ? "OPEN" : "CLOSED");  // ...report
    last21 = v21;                                        // remember
  }
  delay(20);                                             // 20 ms debounce - a good switch prints once per press; a bad one chatters
}
