#include <Adafruit_GFX.h>
#include <Adafruit_GC9A01A.h>
#include <SPI.h>
#include <Wire.h>

#define TFT_CS   1
#define TFT_DC   2
#define TFT_RST  0
#define TFT_SCLK 4
#define TFT_MOSI 3
#define BOOT_BUTTON 9

Adafruit_GC9A01A tft(TFT_CS, TFT_DC, TFT_RST);
GFXcanvas16 canvas(240, 240);

const int MPU_ADDR = 0x68;

const int centerX = 120;
const int centerY = 120;
const int ringRadius = 90;
const int bubbleRadius = 12;
const int crossLength = 90;

const float switchTiltThreshold = 75.0; // degrees from flat -> enter wall mode
const float returnTiltThreshold = 20.0; // degrees from flat -> back to floor mode
const float maxYawDeg = 30.0;           // yaw range mapped to full ring radius

bool wallMode = false;
int yawSourceAxis = -1; // 0 = X is the vertical/pitch axis, 1 = Y is the vertical/pitch axis
float yawAngle = 0;
float gyroBiasX = 0, gyroBiasY = 0;

unsigned long lastUpdate = 0;
bool lastButtonState = HIGH;
unsigned long flashUntil = 0;
const unsigned long flashDuration = 300;

void setup() {
  Serial.begin(115200);
  delay(1500);

  SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);
  tft.begin();
  tft.fillScreen(GC9A01A_BLACK);

  pinMode(BOOT_BUTTON, INPUT_PULLUP);

  Wire.begin(6, 5);
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B);
  Wire.write(0);
  Wire.endTransmission(true);

  calibrateGyroBias();
  lastUpdate = millis();
}

void calibrateGyroBias() {
  const int samples = 200;
  long sumX = 0, sumY = 0;
  for (int i = 0; i < samples; i++) {
    int16_t gx, gy;
    readGyroXY(gx, gy);
    sumX += gx;
    sumY += gy;
    delay(5);
  }
  gyroBiasX = (float)sumX / samples;
  gyroBiasY = (float)sumY / samples;
}

void readGyroXY(int16_t &gx, int16_t &gy) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x43); // GYRO_XOUT_H
  Wire.endTransmission(false);
  Wire.requestFrom(MPU_ADDR, 4, true);
  gx = (Wire.read() << 8) | Wire.read();
  gy = (Wire.read() << 8) | Wire.read();
}

void loop() {
  unsigned long now = millis();
  float dt = (now - lastUpdate) / 1000.0;
  lastUpdate = now;

  // --- Read accel ---
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B);
  Wire.endTransmission(false);
  Wire.requestFrom(MPU_ADDR, 6, true);

  int16_t rawAx = (Wire.read() << 8) | Wire.read();
  int16_t rawAy = (Wire.read() << 8) | Wire.read();
  int16_t rawAz = (Wire.read() << 8) | Wire.read();

  float accelX = rawAx / 16384.0;
  float accelY = rawAy / 16384.0;
  float accelZ = rawAz / 16384.0;

  // --- Read gyro X/Y (used only in wall mode) ---
  int16_t rawGx, rawGy;
  readGyroXY(rawGx, rawGy);
  float gyroX = (rawGx - gyroBiasX) / 131.0; // deg/s
  float gyroY = (rawGy - gyroBiasY) / 131.0;

  // --- Total tilt away from flat ---
  float tiltFromFlat = acos(constrain(accelZ, -1.0, 1.0)) * 180.0 / PI;

  // --- Mode switching with hysteresis ---
  if (!wallMode && tiltFromFlat > switchTiltThreshold) {
    wallMode = true;
    // Steepest axis becomes the vertical axis; works for tilt in any direction.
    yawSourceAxis = (fabs(accelX) >= fabs(accelY)) ? 0 : 1;
    yawAngle = 0; // no floor-mode equivalent to carry over
    flashUntil = millis() + flashDuration;
  } else if (wallMode && tiltFromFlat < returnTiltThreshold) {
    wallMode = false;
    flashUntil = millis() + flashDuration;
  }

  // --- Button: re-zero yaw while in wall mode ---
  bool buttonState = digitalRead(BOOT_BUTTON);
  if (buttonState == LOW && lastButtonState == HIGH) {
    if (wallMode) {
      yawAngle = 0;
      flashUntil = millis() + flashDuration;
    }
    delay(50);
  }
  lastButtonState = buttonState;

  int offsetX, offsetY;
  uint16_t bubbleColor;
  const char* label;

  if (!wallMode) {
      // Floor mode: both axes direct from accelerometer, no drift
      offsetX = (int)(-accelY * ringRadius);
      offsetY = (int)(-accelX * ringRadius);
      bubbleColor = GC9A01A_RED;
      label = "FLOOR";
  } else {
      // Wall mode:
      // Pitch = accelZ, ~0 when the bore is level into the wall, sensitive
      // near zero (unlike the saturated dominant axis). Drives vertical offset.
      // Yaw = integrated gyro on the SAME axis as whichever is vertical --
      // rotation about the gravity-aligned axis is exactly what that axis's
      // own gyro reads. Drives horizontal offset.
      float yawRate = (yawSourceAxis == 0) ? gyroX : gyroY;
      yawAngle += yawRate * dt;

      offsetY = (int)(-accelZ * ringRadius);
      offsetX = (int)((yawAngle / maxYawDeg) * ringRadius);

      label = (yawSourceAxis == 0) ? "WALL (X vert)" : "WALL (Y vert)";
      bubbleColor = GC9A01A_BLUE;
  }

  if (millis() < flashUntil) {
    bubbleColor = GC9A01A_WHITE;
  }

  float dist = sqrt((float)(offsetX * offsetX + offsetY * offsetY));
  if (dist > (ringRadius - bubbleRadius)) {
    float scale = (ringRadius - bubbleRadius) / dist;
    offsetX = (int)(offsetX * scale);
    offsetY = (int)(offsetY * scale);
  }

  int bubbleX = centerX + offsetX;
  int bubbleY = centerY + offsetY;

  // --- Draw ---
  canvas.fillScreen(GC9A01A_BLACK);
  canvas.drawCircle(centerX, centerY, ringRadius, GC9A01A_WHITE);
  canvas.drawLine(centerX - crossLength, centerY, centerX + crossLength, centerY, GC9A01A_WHITE);
  canvas.drawLine(centerX, centerY - crossLength, centerX, centerY + crossLength, GC9A01A_WHITE);
  canvas.fillCircle(bubbleX, bubbleY, bubbleRadius, bubbleColor);

  canvas.setTextSize(1);
  canvas.setTextColor(GC9A01A_WHITE);
  canvas.setCursor(10, 10);
  canvas.print("Tilt: "); canvas.print(tiltFromFlat, 0);

  canvas.setCursor(55, 210);
  canvas.print(label);

  tft.drawRGBBitmap(0, 0, canvas.getBuffer(), 240, 240);

  delay(30);
}