// TEST I — Power bank overnight test with the keep-alive load
// Steps:   1) upload this with the laptop on the UART port;  2) solder/clip the 47 Ohm 1 W resistor across header 5V and GND;
//          3) unplug the laptop, plug the board into the power bank's cable on the POWER micro-USB;  4) leave overnight.
// What it does: keeps the Wi-Fi radio on (adds ~50 mA) and blinks LED D4 once a second so you can see it is alive.
// Pass:    LED still blinking next morning = the bank did not switch itself off.

#include <WiFi.h>                                       // ESP32 Wi-Fi driver (only used to keep the radio powered)

const int PIN_LED = 22;                                 // LED D4 on GPIO22, active-low

void setup() {                                          // runs once
  Serial.begin(115200);                                 // open serial (only visible while the laptop is connected)
  delay(300);                                           // settle
  pinMode(PIN_LED, OUTPUT);                             // LED pin as output
  WiFi.mode(WIFI_STA);                                  // turn the radio on in station mode...
  WiFi.disconnect(false);                               // ...but do not connect to anything; the radio idling is the point
  Serial.println("TEST I: keep-alive running. Move to the power bank and leave overnight.");  // instructions
}

void loop() {                                           // runs forever
  digitalWrite(PIN_LED, LOW);                           // LED on
  delay(100);                                           // for 100 ms
  digitalWrite(PIN_LED, HIGH);                          // LED off
  delay(900);                                           // for 900 ms -> one blink per second, a visible heartbeat
}
