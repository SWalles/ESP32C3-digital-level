#include <Wire.h>

const int MPU_ADDR = 0x68;
unsigned long sampleCount = 0;

// ---- Alpha-beta filter (integer, shift-based) ----
class AlphaBetaFilter {
public:
  AlphaBetaFilter(uint8_t alpha_shift = 5, uint8_t beta_shift = 5)
    : x_est(0), v_est(0), x_rem(0), v_rem(0),
      alpha_shift(alpha_shift), beta_shift(beta_shift), initialized(false) {}

  int32_t update(int32_t measurement) {
    if (!initialized) {
      // Seed the filter with the first real reading instead of ramping up from 0
      x_est = measurement;
      v_est = 0;
      initialized = true;
      return x_est;
    }

    int32_t x_predicted = x_est + v_est;      // dt = 1
    int32_t residual = measurement - x_predicted;

    x_rem += residual;
    int32_t x_corr = x_rem >> alpha_shift;
    x_rem -= x_corr << alpha_shift;

    v_rem += residual;
    int32_t v_corr = v_rem >> beta_shift;
    v_rem -= v_corr << beta_shift;

    x_est = x_predicted + x_corr;
    v_est = v_est + v_corr;

    return x_est;
  }

  int32_t value() const { return x_est; }

private:
  int32_t x_est, v_est, x_rem, v_rem;
  uint8_t alpha_shift, beta_shift;
  bool initialized;
};

// One filter per channel: ax, ay, az, temp, gx, gy, gz
AlphaBetaFilter filters[7] = {
  AlphaBetaFilter(5, 10), // ax
  AlphaBetaFilter(5, 10), // ay
  AlphaBetaFilter(5, 10), // az
  AlphaBetaFilter(5, 10), // temp
  AlphaBetaFilter(5, 10), // gx
  AlphaBetaFilter(5, 10), // gy
  AlphaBetaFilter(5, 10), // gz
};

void setup() {
  Serial.begin(921600);
  delay(1500);

  Wire.begin(6, 5);
  Wire.setClock(400000);

  writeReg(0x6B, 0x00); // PWR_MGMT_1 = 0, wake up
  writeReg(0x1A, 0x01); // CONFIG, DLPF_CFG=1
  writeReg(0x19, 0x00); // SMPLRT_DIV=0 -> 1kHz
  writeReg(0x38, 0x01); // INT_ENABLE, DATA_RDY_EN
}

void writeReg(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(reg);
  Wire.write(val);
  Wire.endTransmission(true);
}

bool dataReady() {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3A);
  Wire.endTransmission(false);
  Wire.requestFrom(MPU_ADDR, 1, true);
  return Wire.read() & 0x01;
}

void loop() {
  if (!dataReady()) return;

  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B);
  Wire.endTransmission(false);
  Wire.requestFrom(MPU_ADDR, 14, true);

  int16_t ax   = (Wire.read() << 8) | Wire.read();
  int16_t ay   = (Wire.read() << 8) | Wire.read();
  int16_t az   = (Wire.read() << 8) | Wire.read();
  int16_t temp = (Wire.read() << 8) | Wire.read();
  int16_t gx   = (Wire.read() << 8) | Wire.read();
  int16_t gy   = (Wire.read() << 8) | Wire.read();
  int16_t gz   = (Wire.read() << 8) | Wire.read();

  sampleCount++;

  // Run each channel through its filter
  int32_t ax_f   = filters[0].update(ax);
  int32_t ay_f   = filters[1].update(ay);
  int32_t az_f   = filters[2].update(az);
  int32_t temp_f = filters[3].update(temp);
  int32_t gx_f   = filters[4].update(gx);
  int32_t gy_f   = filters[5].update(gy);
  int32_t gz_f   = filters[6].update(gz);

  // Build the line in one buffer, one Serial.print call -> fewer USB/UART transactions
  char buf[192];
  int len = snprintf(buf, sizeof(buf),
                      "%lu,%lu,%d,%d,%d,%d,%d,%d,%d,%ld,%ld,%ld,%ld,%ld,%ld,%ld\n",
                      micros(), sampleCount,
                      ax, ay, az, temp, gx, gy, gz,
                      ax_f, ay_f, az_f, temp_f, gx_f, gy_f, gz_f);
  Serial.write(buf, len);
}