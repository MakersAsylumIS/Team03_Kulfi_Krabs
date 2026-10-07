// TEST A1 — Board alive: Serial output + blink the onboard LED D4
// Connect: only the micro-USB marked UART to the laptop. Nothing else.
// Pass:    Serial Monitor (115200) prints "alive" every second and LED D4 blinks with it.

const int PIN_LED = 22;                 // GPIO22 drives the green LED D4 on the board (LOW = on)

void setup() {                          // runs once after reset
  Serial.begin(115200);                 // open the USB serial link at 115200 baud
  delay(300);                           // give the serial port a moment to come up
  pinMode(PIN_LED, OUTPUT);             // make GPIO22 an output so we can drive the LED
  Serial.println("TEST A1: board is alive, LED D4 should blink");  // first message proves serial works
}

void loop() {                           // runs forever after setup()
  digitalWrite(PIN_LED, LOW);           // LED on (the LED is wired active-low on this board)
  Serial.println("alive - LED on");     // report the state
  delay(500);                           // wait half a second
  digitalWrite(PIN_LED, HIGH);          // LED off
  Serial.println("alive - LED off");    // report the state
  delay(500);                           // wait half a second, then repeat
}
