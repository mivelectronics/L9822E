\
#include <Arduino.h>
#include <SPI.h>
#include <L9822E.h>

// Single device example (any board)
L9822E drv(/*CE*/10, /*RESET*/-1);

void setup() {
  SPI.begin();
  drv.begin(SPI, 1000000);

  drv.setChannel(0, 0, true);
  drv.setChannel(0, 1, true);
  drv.write();
}

void loop() {}
