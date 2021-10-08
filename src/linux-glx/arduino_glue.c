#include "arduino_glue.h"
#include <unistd.h>
#include <sys/time.h>

unsigned long millis() {
  struct timeval tv={0};
  gettimeofday(&tv,0);
  return (unsigned long)tv.tv_sec*1000ul+tv.tv_usec/1000;
}

void delay(unsigned long ms) {
  usleep(ms*1000);
}
