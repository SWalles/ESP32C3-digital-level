#include <Wire.h>

const int MPU_ADDR = 0x68;

void setup() {
  Wire.begin(6, 7);       // SDA, SCL
  Wire.setClock(100000);
  Serial.begin(115200);
  delay(1500);

  // Wake up the MPU6050 (it starts in sleep mode by default)
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B);  // PWR_MGMT_1 register
  Wire.write(0);     // set to 0 to wake up
  Wire.endTransmission(true);

  Serial.println("MPU6050 direct register read starting...");
}

void loop() {
  // Burst-read 14 bytes starting at ACCEL_XOUT_H (0x3B):
  // accel X,Y,Z (6 bytes) + temp (2 bytes) + gyro X,Y,Z (6 bytes)
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B);
  Wire.endTransmission(false);
  Wire.requestFrom(MPU_ADDR, 14, true);

  int16_t rawAx = (Wire.read() << 8) | Wire.read();
  int16_t rawAy = (Wire.read() << 8) | Wire.read();
  int16_t rawAz = (Wire.read() << 8) | Wire.read();
  int16_t rawTemp = (Wire.read() << 8) | Wire.read();
  int16_t rawGx = (Wire.read() << 8) | Wire.read();
  int16_t rawGy = (Wire.read() << 8) | Wire.read();
  int16_t rawGz = (Wire.read() << 8) | Wire.read();

  // Convert to real units.
  // Default ranges: accel ±2g -> 16384 LSB/g, gyro ±250°/s -> 131 LSB/(°/s)
  float accelX = rawAx / 16384.0;
  float accelY = rawAy / 16384.0;
  float accelZ = rawAz / 16384.0;

  float gyroX = rawGx / 131.0;
  float gyroY = rawGy / 131.0;
  float gyroZ = rawGz / 131.0;

  float tempC = (rawTemp / 340.0) + 36.53;

  Serial.print("Accel (g)    X: ");
  Serial.print(accelX, 2);
  Serial.print("  Y: ");
  Serial.print(accelY, 2);
  Serial.print("  Z: ");
  Serial.println(accelZ, 2);

  Serial.print("Gyro (deg/s) X: ");
  Serial.print(gyroX, 2);
  Serial.print("  Y: ");
  Serial.print(gyroY, 2);
  Serial.print("  Z: ");
  Serial.println(gyroZ, 2);

  Serial.print("Temp (C): ");
  Serial.println(tempC, 2);

  Serial.println("---");

  delay(500);
}