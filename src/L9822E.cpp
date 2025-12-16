\
#include "L9822E.h"

static inline bool _pinsProvided(int8_t a, int8_t b, int8_t c) {
  return (a >= 0) && (b >= 0) && (c >= 0);
}

L9822EChain::L9822EChain(uint8_t deviceCount, int8_t cePin, int8_t resetPin)
: _n(deviceCount), _ce(cePin), _rst(resetPin) {
  if (_n == 0) _n = 1;
  _ctrl = new uint8_t[_n];
  _diag = new uint8_t[_n];
  for (uint8_t i = 0; i < _n; i++) {
    _ctrl[i] = 0xFF; // all OFF by default (SI=1 -> OFF)
    _diag[i] = 0x00;
  }
}

L9822EChain::~L9822EChain() {
  delete[] _ctrl;
  delete[] _diag;
}

bool L9822EChain::begin(SPIClass &spi, uint32_t clockHz, int8_t sck, int8_t miso, int8_t mosi) {
  _spi = &spi;
  _settings = SPISettings(clockHz, MSBFIRST, SPI_MODE1);

  pinMode(_ce, OUTPUT);
  digitalWrite(_ce, HIGH); // deselect (CE active low)

  if (_rst >= 0) {
    pinMode(_rst, OUTPUT);
    digitalWrite(_rst, HIGH);
  }

  // On ESP32/ESP32-S3, SPI.begin(sck, miso, mosi, ss) or SPI.begin(sck, miso, mosi) is supported.
  // We only call it if pins are explicitly provided.
  if (_pinsProvided(sck, miso, mosi)) {
    _spi->begin(sck, miso, mosi);
  } else {
    // fallback: user may have called SPI.begin() already, or platform has fixed pins
    _spi->begin();
  }

  // Optional reset pulse
  if (_rst >= 0) hardwareReset();
  return true;
}

void L9822EChain::hardwareReset(uint16_t lowMs, uint16_t postMs) {
  if (_rst < 0) return;
  digitalWrite(_rst, LOW);   // RESET is active-low
  delay(lowMs);
  digitalWrite(_rst, HIGH);
  delay(postMs);
}

void L9822EChain::setChannel(uint8_t deviceIndex, uint8_t channel, bool on) {
  if (deviceIndex >= _n || channel > 7) return;
  uint8_t mask = (uint8_t)(1u << channel);
  if (on) {
    _ctrl[deviceIndex] &= (uint8_t)~mask; // 0 -> ON
  } else {
    _ctrl[deviceIndex] |= mask;           // 1 -> OFF
  }
}

void L9822EChain::setAll(bool on) {
  uint8_t v = on ? 0x00 : 0xFF;
  for (uint8_t i = 0; i < _n; i++) _ctrl[i] = v;
}

void L9822EChain::setRaw(uint8_t deviceIndex, uint8_t controlByte) {
  if (deviceIndex >= _n) return;
  _ctrl[deviceIndex] = controlByte;
}

uint8_t L9822EChain::getRaw(uint8_t deviceIndex) const {
  if (deviceIndex >= _n) return 0xFF;
  return _ctrl[deviceIndex];
}

void L9822EChain::_transfer(uint8_t *rxOut) {
  if (!_spi) return;

  digitalWrite(_ce, LOW);          // CE falling edge loads diagnostic bits into shift register
  _spi->beginTransaction(_settings);

  // IMPORTANT for daisy-chain:
  // transmit farthest device first so that after 8*N clocks each device ends up with its intended byte.
  for (int i = (int)_n - 1; i >= 0; --i) {
    uint8_t rx = _spi->transfer(_ctrl[i]);
    if (rxOut) rxOut[i] = rx;
  }

  _spi->endTransaction();
  digitalWrite(_ce, HIGH);         // CE rising edge latches shift-register bits into output latch (applies outputs)
}

void L9822EChain::write() {
  _transfer(nullptr);
}

void L9822EChain::writeRead(uint8_t *diagOut) {
  _transfer(_diag);
  if (diagOut) {
    for (uint8_t i = 0; i < _n; i++) diagOut[i] = _diag[i];
  }
}

L9822EChain::Faults L9822EChain::checkFaults(uint32_t settleDelayUs) {
  // 1) apply outputs
  write();

  // The datasheet specifies an internal delay between CE L->H and fault-reset behavior (tUD),
  // and outputs need some time to settle electrically. Provide a small delay before sampling diagnostics.
  if (settleDelayUs) delayMicroseconds(settleDelayUs);

  // 2) re-send the same command and read back diagnostics corresponding to the previous output states
  writeRead(nullptr);

  Faults f;
  f.deviceCount = _n;

  for (uint8_t dev = 0; dev < _n; dev++) {
    uint8_t cmd = _ctrl[dev];
    uint8_t d   = _diag[dev];

    for (uint8_t ch = 0; ch < 8; ch++) {
      uint8_t bit = (uint8_t)(1u << ch);
      bool cmdOff = (cmd & bit) != 0;  // 1=OFF
      bool outHigh = (d & bit) != 0;  // 1=output high (>~1.8V), 0=output low

      uint8_t global = (uint8_t)(dev * 8 + ch);
      uint64_t gmask = (uint64_t)1u << global;

      // Decode per datasheet guidance:
      // - If OFF commanded, but diagnostic returns 0 => output floating/low => open circuit (no pull-up through load).
      // - If ON commanded, but diagnostic returns 1 => output didn't go low (out-of-sat, short, fault turn-off).
      if (cmdOff && !outHigh) f.openCircuitMask |= gmask;
      if (!cmdOff && outHigh) f.shortOrFaultMask |= gmask;
    }
  }
  return f;
}
