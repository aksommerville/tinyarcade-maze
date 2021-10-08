#ifndef ARDUINO_GLUE_H
#define ARDUINO_GLUE_H

unsigned long millis();
void delay(unsigned long ms);

// The shared 'main.c' implements these. We must call them like an Arduino.
void setup();
void loop();

#endif
