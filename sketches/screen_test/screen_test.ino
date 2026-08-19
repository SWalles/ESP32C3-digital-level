#include <Adafruit_GFX.h>
#include <Adafruit_GC9A01A.h>
#include <SPI.h>
#include <Wire.h>

#define TFT_CS   1
#define TFT_DC   2
#define TFT_RST  0
#define TFT_SCLK 4
#define TFT_MOSI 3

Adafruit_GC9A01A tft(TFT_CS, TFT_DC, TFT_RST);
GFXcanvas16 canvas(240, 240);

const int MPU_ADDR = 0x68;

const int centerX = 120;
const int centerY = 120;
const int ringRadius = 90;
const int bubbleRadius = 12;
const int crossLength = 90;

// Thresholds for switching away from flat mode (with hysteresis to avoid flicker at the boundary)
const float switchThreshold = 0.80;  // switch to side view once axis exceeds this
const float returnThreshold = 0.65;  // must drop below this to return to flat view

int currentMode = 2; // 0 = X dominant, 1 = Y dominant, 2 = flat (XY)

unsigned long flashUntil = 0;
const unsigned long flashDuration = 400;

void setup() {
  Serial.begin(115200);
  delay(1500);

  SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);
  tft.begin();
  tft.fillScreen(GC9A01A_BLACK);

  Wire.begin(6, 5);
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B);
  Wire.write(0);
  Wire.endTransmission(true);
}

void loop() {
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

  float absX = fabs(accelX);
  float absY = fabs(accelY);

  // Decide mode with hysteresis so it doesn't flicker near the threshold
  if (currentMode == 2) {
    // Currently flat -> only leave if X or Y clearly exceeds threshold
    if (absX > switchThreshold && absX >= absY) currentMode = 0;
    else if (absY > switchThreshold && absY > absX) currentMode = 1;
  } else if (currentMode == 0) {
    // Currently X-dominant -> only return to flat if X drops back down
    if (absX < returnThreshold) currentMode = 2;
  } else if (currentMode == 1) {
    // Currently Y-dominant -> only return to flat if Y drops back down
    if (absY < returnThreshold) currentMode = 2;
  }

  int offsetX, offsetY;
  uint16_t bubbleColor;
  const char* label;

  if (currentMode == 2) { // flat -> use X/Y
    offsetX = (int)(-accelY * ringRadius);
    offsetY = (int)(-accelX * ringRadius);
    bubbleColor = GC9A01A_RED;
    label = "XY (flat)";
  } else if (currentMode == 0) { // X tipped -> use Y/Z
    offsetX = (int)(-accelZ * ringRadius);
    offsetY = (int)(-accelY * ringRadius);
    bubbleColor = GC9A01A_GREEN;
    label = "YZ (on X side)";
  } else { // Y tipped -> use X/Z
    offsetX = (int)(-accelZ * ringRadius);
    offsetY = (int)(-accelX * ringRadius);
    bubbleColor = GC9A01A_BLUE;
    label = "XZ (on Y side)";
  }

  static int lastMode = -1;
  if (currentMode != lastMode) {
    flashUntil = millis() + flashDuration;
    lastMode = currentMode;
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

  canvas.fillScreen(GC9A01A_BLACK);
  canvas.drawCircle(centerX, centerY, ringRadius, GC9A01A_WHITE);
  canvas.drawLine(centerX - crossLength, centerY, centerX + crossLength, centerY, GC9A01A_WHITE);
  canvas.drawLine(centerX, centerY - crossLength, centerX, centerY + crossLength, GC9A01A_WHITE);
  canvas.fillCircle(bubbleX, bubbleY, bubbleRadius, bubbleColor);

  canvas.setTextSize(1);
  canvas.setTextColor(GC9A01A_WHITE);
  canvas.setCursor(70, 20);
  canvas.print("X: "); canvas.print(accelX, 2);
  canvas.setCursor(70, 32);
  canvas.print("Y: "); canvas.print(accelY, 2);
  canvas.setCursor(70, 44);
  canvas.print("Z: "); canvas.print(accelZ, 2);

  canvas.setCursor(55, 210);
  canvas.print(label);

  tft.drawRGBBitmap(0, 0, canvas.getBuffer(), 240, 240);

  delay(50);
}