\
#include <Arduino.h>
#include <SPI.h>
#include <L9822E.h>

// Your wiring (ESP32-S3):
// RESET - GPIO36
// CE    - GPIO35
// SI    - GPIO37 (MOSI)
// SO    - GPIO38 (MISO)
// CLK   - GPIO48 (SCK)

static constexpr int PIN_RESET = 36;
static constexpr int PIN_CE    = 35;
static constexpr int PIN_MOSI  = 37;
static constexpr int PIN_MISO  = 38;
static constexpr int PIN_SCK   = 48;

L9822EChain chain(2, PIN_CE, PIN_RESET);

void setup() {
  Serial.begin(115200);

  // Init SPI with custom pins (ESP32 Arduino supports this form)
  SPI.begin(PIN_SCK, PIN_MISO, PIN_MOSI);

  // Begin driver (MODE1, up to 2MHz per datasheet; start with 1MHz)
  chain.begin(SPI, 1000000);

  // Turn ON some channels:
  // device 0 = closest to ESP32 (first in chain)
  // device 1 = farthest in chain
  chain.setChannel(0, 0, true); // dev0 OUT0 ON
  chain.setChannel(0, 7, true); // dev0 OUT7 ON
  chain.setChannel(1, 3, true); // dev1 OUT3 ON

  chain.write();

  Serial.println("Outputs applied.");
}

void loop() {
  // Example: periodic fault check (interpretation depends on your load wiring)
  auto faults = chain.checkFaults(300);

  if (faults.openCircuitMask || faults.shortOrFaultMask) {
    Serial.printf("OpenMask : 0x%llX\n", (unsigned long long)faults.openCircuitMask);
    Serial.printf("ShortMask: 0x%llX\n", (unsigned long long)faults.shortOrFaultMask);
  }

  delay(1000);
}
