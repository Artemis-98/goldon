/*
 * اسکن DHT روی GPIOهای آزاد ESP32-S3
 * هر پین را با DHT11 و DHT22 امتحان می‌کند.
 */

#include <DHT.h>

// پین‌های آزاد / محتمل برای DATA (از استفاده‌شده‌ها پرهیز می‌کنیم)
// استفاده شده پروژه: 1,2,4,7,9,10,11,16,17,18
// با این حال همه را هم اسکن می‌کنیم چون سیم ممکن است جابه‌جا باشد
static const int PINS[] = {
  1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
  16, 17, 18, 19, 20, 21, 35, 36, 37, 38, 39, 40, 41, 42, 47, 48
};
static const int PIN_COUNT = sizeof(PINS) / sizeof(PINS[0]);

bool tryPin(int pin, uint8_t type, const char *typeName) {
  DHT dht(pin, type);
  dht.begin();
  delay(1200);
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  if (!isnan(h) && !isnan(t)) {
    Serial.printf(">>> FOUND %s on GPIO %d  T=%.1f C  H=%.1f %%\n", typeName, pin, t, h);
    return true;
  }
  return false;
}

void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.println();
  Serial.println("====================================");
  Serial.println(" DHT GPIO SCAN — all candidate pins");
  Serial.println("====================================");
}

void loop() {
  int found = 0;
  for (int i = 0; i < PIN_COUNT; i++) {
    int pin = PINS[i];
    Serial.printf("scan GPIO %d ...\n", pin);
    if (tryPin(pin, DHT11, "DHT11")) found++;
    else if (tryPin(pin, DHT22, "DHT22")) found++;
    else Serial.printf("  GPIO %d: no\n", pin);
  }
  Serial.printf("\n===== SCAN DONE  found=%d =====\n", found);
  if (found == 0) {
    Serial.println("هيچ پيني جواب نداد. VCC=3.3V و GND را چک کن.");
  }
  Serial.println("اسکن دوباره بعد از 5 ثانيه...\n");
  delay(5000);
}
