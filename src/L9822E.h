\
#pragma once
#include <Arduino.h>
#include <SPI.h>

/**
 * L9822EChain - control N cascaded (daisy-chained) L9822E devices on one CE line.
 *
 * Datasheet behavior leveraged:
 * - OUT7 (MSB) is transferred first, OUT0 last.
 * - SI=0 -> ON, SI=1 -> OFF.
 * - SI latched on SCLK falling edge, SO changes on SCLK rising edge -> SPI MODE1.
 * - Falling edge of CE loads diagnostics into shift register; rising edge of CE latches new control bits to outputs.
 *
 * Indexing:
 * - deviceIndex 0 = device closest to MCU MOSI (first in chain).
 * - deviceIndex N-1 = farthest device.
 *
 * The library handles the required transmit order automatically.
 */
class L9822EChain {
public:
  struct Faults {
    uint64_t openCircuitMask = 0;      // bit (dev*8 + ch) = 1 means open-circuit (OFF commanded, but diag came back low)
    uint64_t shortOrFaultMask = 0;     // bit (dev*8 + ch) = 1 means out-of-sat/short/fault (ON commanded, but diag came back high)
    uint8_t deviceCount = 0;
  };

  L9822EChain(uint8_t deviceCount, int8_t cePin, int8_t resetPin = -1);
  ~L9822EChain();

  /**
   * begin()
   * Optionally you can pass SCK/MISO/MOSI pins (ESP32 style). If any pin is -1, this function will not call spi.begin(...).
   */
  bool begin(SPIClass &spi = SPI, uint32_t clockHz = 1000000, int8_t sck = -1, int8_t miso = -1, int8_t mosi = -1);

  void hardwareReset(uint16_t lowMs = 2, uint16_t postMs = 2);

  // Channel control
  void setChannel(uint8_t deviceIndex, uint8_t channel /*0..7*/, bool on);
  void setAll(bool on);
  void setRaw(uint8_t deviceIndex, uint8_t controlByte); // 1=OFF, 0=ON
  uint8_t getRaw(uint8_t deviceIndex) const;

  // Transfers
  void write(); // apply current control bytes, ignore returned diag
  void writeRead(uint8_t *diagOut /*len=deviceCount*/); // apply and return diagnostics from the *previous* output states

  // Convenience diagnostic helper: write -> delay -> writeRead -> decode faults vs last commanded state
  Faults checkFaults(uint32_t settleDelayUs = 300);

  uint8_t devices() const { return _n; }
  const uint8_t* lastDiag() const { return _diag; }

private:
  void _transfer(uint8_t *rxOut /*len=_n*/);

  uint8_t _n = 0;
  int8_t _ce = -1;
  int8_t _rst = -1;

  SPIClass *_spi = nullptr;
  SPISettings _settings = SPISettings(1000000, MSBFIRST, SPI_MODE1);

  uint8_t *_ctrl = nullptr;  // per device, 1=OFF,0=ON
  uint8_t *_diag = nullptr;  // last received diagnostics per device (same indexing as _ctrl)
};

/**
 * Convenience single-device wrapper (deviceCount=1).
 */
class L9822E : public L9822EChain {
public:
  L9822E(int8_t cePin, int8_t resetPin = -1) : L9822EChain(1, cePin, resetPin) {}
};
