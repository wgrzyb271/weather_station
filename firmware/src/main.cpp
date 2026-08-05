#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <cstdio>
#include <cmath>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME680.h>

// --- PIN DEFINITIONS ---
#define TFT_SCLK  19
#define TFT_MOSI  18
#define TFT_CS    21
#define TFT_DC    16
#define TFT_RST   17

#define ADC_PIN   0
#define BL_PIN    1

// --- HARDWARE SPECIFIC ADC SETTINGS (LDR pull-up / resistor pull-down divider) ---
// Wiring: Vcc -> LDR -> ADC_PIN -> R_pulldown -> GND
// Bright light = LDR resistance drops   = node pulled toward Vcc  = HIGH raw ADC
// Dark room    = LDR resistance rises   = node pulled toward GND  = LOW raw ADC
#define ADC_RAW_DARK    150   // Adjust based on Serial log reading in total darkness
#define ADC_RAW_BRIGHT  3600  // Adjust based on Serial log reading under bright light

// --- IEEE 1789 & HARDWARE PWM CONFIGURATION ---
// IEEE Std 1789-2015 does NOT define a single "safe" frequency - it defines a
// risk curve relating modulation frequency to percent flicker (modulation depth):
//   < 90 Hz            : high risk region, avoid regardless of depth
//   90 Hz  - 1250 Hz    : acceptable only at reduced modulation depth
//   > 1250-3000 Hz      : commonly cited as the "low risk / no observable effect"
//                         band even near 100% modulation depth, since above
//                         ~3 kHz the frequency is well past where stroboscopic
//                         and phantom-array effects remain perceptible.
// 24 kHz PWM Frequency:
// 1. Clears every published threshold above (1250 Hz / 2000 Hz / 3000 Hz) by
//    an 8-24x margin, independent of the exact modulation depth achieved below.
// 2. Sits safely above 20 kHz, the traditional edge of adult human hearing,
//    giving margin against audible coil/capacitor whine (20 kHz itself is
//    borderline - some younger listeners perceive close to that edge).
#define PWM_FREQ         24000  // 24 kHz (comfortable margin above IEEE 1789 low-risk band and audible range)
#define PWM_RES          8      // 8-bit resolution (0 - 255)
#define PWM_MIN_THRESHOLD 15    // Duty cycle floor (prevents display dropouts).
                                 // NOTE: this caps modulation depth at ~88.9%
                                 // (Percent Flicker = 100*(255-15)/(255+15)),
                                 // not full 0-100% - a small extra safety
                                 // margin on top of the frequency margin above.

// --- TIMING INTERVALS (Non-Blocking Scheduler) ---
constexpr uint32_t BACKLIGHT_INTERVAL_MS = 20;   // 50 Hz Backlight & Light sensor update rate
constexpr uint32_t SENSOR_INTERVAL_MS    = 2000; // 0.5 Hz Weather updates

// --- GLOBAL OBJECTS ---
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);
Adafruit_BME680 bme(&Wire);

// --- STATE MANAGEMENT ---
struct SensorData {
  float temp = 0.0f;
  float hum  = 0.0f;
  float press = 0.0f;
  float gas  = 0.0f;
} currentReadings;

char strBuffer[32];
float filteredAdc = 0.0f; // EMA low-pass filter state variable

// --- HELPER FUNCTIONS ---
const char* evaluateGasQuality(float gasResistance) {
  if (std::isnan(gasResistance) || gasResistance <= 0.0f) return "WarmUp";
  if (gasResistance >= 150000.0f) return "Swietna";
  if (gasResistance >= 80000.0f)  return "Dobra";
  if (gasResistance >= 40000.0f)  return "Srednia";
  if (gasResistance >= 15000.0f)  return "Zla";
  return "Terrible";
}

uint16_t getGasColor(float gasResistance) {
  if (std::isnan(gasResistance) || gasResistance <= 0.0f) return ST77XX_YELLOW;
  if (gasResistance >= 80000.0f)  return ST77XX_GREEN;
  if (gasResistance >= 40000.0f)  return ST77XX_YELLOW;
  if (gasResistance >= 15000.0f)  return ST77XX_ORANGE;
  return ST77XX_RED;
}

void initDisplayUI() {
  tft.initR(INITR_BLACKTAB);
  tft.setRotation(1);
  tft.fillScreen(ST77XX_BLACK);
  tft.drawRect(2, 2, 156, 124, ST77XX_WHITE);
  tft.setTextWrap(false);

  // Header
  tft.setTextColor(ST77XX_CYAN);
  tft.setTextSize(1);
  tft.setCursor(20, 10);
  tft.println("STACJA METEO BME680");
  tft.drawFastHLine(5, 22, 150, ST77XX_CYAN);

  // Static Labels
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(10, 35); tft.print("Temperatura:");
  tft.setCursor(10, 55); tft.print("Wilgotnosc :");
  tft.setCursor(10, 75); tft.print("Cisnienie  :");
  tft.setCursor(10, 95); tft.print("Jak. pow.  :");
}

// --- TASK MANAGERS ---
void updateBacklightTask() {
  int rawAdc = analogRead(ADC_PIN);

  // Exponential Moving Average filter (Alpha = 0.05) to eliminate high-frequency noise
  filteredAdc = (0.05f * rawAdc) + (0.95f * filteredAdc);

  // ---------------------------------------------------------
  // MAPPING logic for LDR pull-up divider:
  // Low raw ADC  (dark room)   -> 255 (full brightness)
  // High raw ADC (bright room) -> PWM_MIN_THRESHOLD (dim)
  // ---------------------------------------------------------
  int targetPwm = map((int)filteredAdc, ADC_RAW_DARK, ADC_RAW_BRIGHT, 255, PWM_MIN_THRESHOLD);

  targetPwm = constrain(targetPwm, PWM_MIN_THRESHOLD, 255);
  snprintf(strBuffer, sizeof(strBuffer), "%.1d", targetPwm);
  Serial.println(strBuffer);

  // Write PWM duty cycle using ESP32 LEDC peripheral (frequency fixed at
  // PWM_FREQ regardless of duty - only duty cycle varies, never frequency,
  // which avoids the known failure mode where cheap dimmers drop PWM
  // frequency at low brightness and reintroduce flicker risk).
  ledcWrite(BL_PIN, targetPwm);
}

void updateSensorTask() {
  if (!bme.performReading()) {
    Serial.println("[ERROR] BME680 Read Failed!");
    return;
  }

  currentReadings.temp  = bme.temperature;
  currentReadings.hum   = bme.humidity;
  currentReadings.press = bme.pressure / 100.0f;
  currentReadings.gas   = bme.gas_resistance;

  // 1. Temperature
  tft.setTextColor(ST77XX_RED, ST77XX_BLACK);
  tft.setCursor(90, 35);
  snprintf(strBuffer, sizeof(strBuffer), "%.1f C ", currentReadings.temp);
  tft.print(strBuffer);

  // 2. Humidity
  tft.setTextColor(ST77XX_BLUE, ST77XX_BLACK);
  tft.setCursor(90, 55);
  snprintf(strBuffer, sizeof(strBuffer), "%.1f %% ", currentReadings.hum);
  tft.print(strBuffer);

  // 3. Pressure
  tft.setTextColor(ST77XX_GREEN, ST77XX_BLACK);
  tft.setCursor(90, 75);
  snprintf(strBuffer, sizeof(strBuffer), "%.1f hPa", currentReadings.press);
  tft.print(strBuffer);

  // 4. Gas / Air Quality
  const char* statusStr = evaluateGasQuality(currentReadings.gas);
  uint16_t statusColor = getGasColor(currentReadings.gas);

  tft.setTextColor(statusColor, ST77XX_BLACK);
  tft.setCursor(90, 95);
  snprintf(strBuffer, sizeof(strBuffer), "%-9s", statusStr);
  tft.print(strBuffer);

  // Telemetry output over Serial
  Serial.printf("[TELEMETRY] T:%.1f C | H:%.1f %% | P:%.1f hPa | Gas: %s (%.0f Ohm) | Light ADC: %.0f\n",
                currentReadings.temp, currentReadings.hum, currentReadings.press, 
                statusStr, std::isnan(currentReadings.gas) ? 0.0f : currentReadings.gas, filteredAdc);
}

// --- SETUP & MAIN LOOP ---
void setup() {
  Serial.begin(115200);

  // Attach hardware LEDC PWM timer for IEEE 1789 compliance (24 kHz, 8-bit)
  ledcAttach(BL_PIN, PWM_FREQ, PWM_RES);

  Wire.begin();
  initDisplayUI();

  if (!bme.begin()) {
    Serial.println("[CRITICAL] BME680 Sensor initialization failed!");
    tft.drawRect(2, 2, 156, 124, ST77XX_RED);
    tft.setTextColor(ST77XX_RED);
    tft.setTextSize(1);
    tft.setCursor(20, 50);
    tft.println("CRITICAL ERROR:");
    tft.setCursor(20, 65);
    tft.println("BME680 NOT FOUND");
    while (true) { delay(100); }
  }

  // Sensor parameters
  bme.setTemperatureOversampling(BME680_OS_8X);
  bme.setHumidityOversampling(BME680_OS_2X);
  bme.setPressureOversampling(BME680_OS_4X);
  bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
  bme.setGasHeater(320, 150); // 320°C for 150 ms

  // Pre-seed ADC filter with initial pin reading
  filteredAdc = analogRead(ADC_PIN);
}

void loop() {
  uint32_t currentMillis = millis();
  static uint32_t lastBacklightUpdate = 0;
  static uint32_t lastSensorUpdate    = 0;

  // Task 1: Non-blocking Light sensing & PWM adjustment (50 Hz)
  if (currentMillis - lastBacklightUpdate >= BACKLIGHT_INTERVAL_MS) {
    lastBacklightUpdate = currentMillis;
    updateBacklightTask();
  }

  // Task 2: Non-blocking Environmental Measurement processing (0.5 Hz)
  if (currentMillis - lastSensorUpdate >= SENSOR_INTERVAL_MS) {
    lastSensorUpdate = currentMillis;
    updateSensorTask();
  }
}