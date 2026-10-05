#include <Wire.h>

const int MPU_ADDR = 0x68;
const int INT_PIN  = 4; // Connect MPU-6050 INT pin to GPIO 4
unsigned long sampleCount = 0;

// ---- Fixed-Point Integer Exponential Moving Average (EMA) ----
class FastEMA {
public:
  FastEMA(uint8_t shift = 7) : shift(shift), state(0), initialized(false) {}

  int32_t update(int16_t raw) {
    int32_t input = (int32_t)raw;
    if (!initialized) {
      state = input << shift; 
      initialized = true;
      return input;
    }
    // Fixed-point EMA recurrence
    state += input - (state >> shift);
    return state >> shift;
  }

  void reset() { initialized = false; }

private:
  uint8_t shift;
  int32_t state;
  bool initialized;
};

// One filter per channel matching original AlphaBetaFilter index order:
// 0: ax, 1: ay, 2: az, 3: temp, 4: gx, 5: gy, 6: gz
FastEMA filters[7] = {
  FastEMA(7), // ax
  FastEMA(7), // ay
  FastEMA(7), // az
  FastEMA(7), // temp
  FastEMA(7), // gx
  FastEMA(7), // gy
  FastEMA(7)  // gz
};

void writeReg(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(reg);
  Wire.write(val);
  Wire.endTransmission(true);
}

void setup() {
  Serial.begin(921600);
  pinMode(INT_PIN, INPUT);

  Wire.begin(6, 5);
  Wire.setClock(400000);
  Wire.setTimeOut(1000); // Prevent I2C bus lockup

  writeReg(0x6B, 0x01); // Wake up & set clock source to Gyro-X PLL
  writeReg(0x1A, 0x01); // DLPF_CFG = 1 (184Hz accel / 188Hz gyro bandwidth)
  writeReg(0x19, 0x00); // SMPLRT_DIV = 0 -> 1kHz output rate
  writeReg(0x38, 0x01); // INT_ENABLE: Data Ready interrupt
}

void loop() {
  // Read hardware INT pin state directly
  if (digitalRead(INT_PIN) == LOW) return;

  // Requesting 14 bytes starting at 0x3B clears data-ready INT automatically
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B);
  Wire.endTransmission(false);
  Wire.requestFrom(MPU_ADDR, 14, true);

  if (Wire.available() < 14) return;

  // Explicit casting avoids sign-extension bugs during 16-bit shift
  int16_t ax   = ((int16_t)Wire.read() << 8) | Wire.read();
  int16_t ay   = ((int16_t)Wire.read() << 8) | Wire.read();
  int16_t az   = ((int16_t)Wire.read() << 8) | Wire.read();
  int16_t temp = ((int16_t)Wire.read() << 8) | Wire.read();
  int16_t gx   = ((int16_t)Wire.read() << 8) | Wire.read();
  int16_t gy   = ((int16_t)Wire.read() << 8) | Wire.read();
  int16_t gz   = ((int16_t)Wire.read() << 8) | Wire.read();

  sampleCount++;

  // Update filters
  int32_t ax_f   = filters[0].update(ax);
  int32_t ay_f   = filters[1].update(ay);
  int32_t az_f   = filters[2].update(az);
  int32_t temp_f = filters[3].update(temp);
  int32_t gx_f   = filters[4].update(gx);
  int32_t gy_f   = filters[5].update(gy);
  int32_t gz_f   = filters[6].update(gz);

  // Exact 16-field CSV output matching original script format:
  // micros, sampleCount, ax, ay, az, temp, gx, gy, gz, ax_f, ay_f, az_f, temp_f, gx_f, gy_f, gz_f
  char buf[256];
  int len = snprintf(buf, sizeof(buf),
                     "%lu,%lu,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
                     micros(), 
                     sampleCount,
                     (int)ax, (int)ay, (int)az, (int)temp, (int)gx, (int)gy, (int)gz,
                     (int)ax_f, (int)ay_f, (int)az_f, (int)temp_f, (int)gx_f, (int)gy_f, (int)gz_f);

  if (len > 0) {
    Serial.write((uint8_t*)buf, len);
  }
}