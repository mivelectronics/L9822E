# L9822E (Arduino / PlatformIO)

Library for **ST L9822E** octal low-side driver with **SPI** interface. Includes **daisy-chain** support (N devices on one CE line).

## Key points (from datasheet)
- **OUT7 (bit 7 / MSB) is shifted first**, OUT0 last.
- **SI=0 → output ON**, **SI=1 → output OFF**.
- **SO updates on SCLK rising edge**, **SI is latched on SCLK falling edge** → use **SPI MODE1**.
- **CE is active-low**. Falling edge of CE loads diagnostic bits into the shift register; rising edge of CE latches new control data to outputs.
- SPI max operating frequency is **2 MHz**.

## Daisy-chain indexing used by this library
`deviceIndex = 0` is the **first** chip connected to the MCU (closest to MOSI).
`deviceIndex = N-1` is the **farthest** chip in the chain.

The library automatically sends bytes in the correct order for a cascaded chain.

## ESP32-S3 note (very important)
The L9822E uses **5V logic (VCC nominal 5V)** and its input thresholds are specified relative to VCC.
In many cases **3.3V MCU signals do NOT meet VIH**. Use a proper level shifter/buffer for **CE, SCLK, SI, RESET** (e.g. 74HCT family).

## Quick usage
```cpp
#include <L9822E.h>

L9822EChain chain(2, /*CE*/35, /*RESET*/36);

void setup() {
  SPI.begin(/*SCK*/48, /*MISO*/38, /*MOSI*/37);
  chain.begin(SPI, 1000000); // 1MHz, MODE1
  chain.setChannel(0, 0, true); // dev0 OUT0 ON
  chain.setChannel(1, 7, true); // dev1 OUT7 ON
  chain.write();
}
```

See `examples/` for more.
