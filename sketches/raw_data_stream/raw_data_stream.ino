#include <Wire.h>

const int MPU_ADDR = 0x68;
unsigned long sampleCount = 0;

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

  // Build the line in one buffer, one Serial.print call -> fewer USB/UART transactions
  char buf[96];
  int len = snprintf(buf, sizeof(buf), "%lu,%lu,%d,%d,%d,%d,%d,%d,%d\n",
                      micros(), sampleCount, ax, ay, az, temp, gx, gy, gz);
  Serial.write(buf, len);
}