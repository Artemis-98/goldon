/*
 * اسکن ADC برای پیدا کردن سنسورهای خاک
 */

static const int PINS[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18};
static const int N = sizeof(PINS) / sizeof(PINS[0]);

void setup() {
  Serial.begin(115200);
  delay(1000);
  analogReadResolution(12);
  Serial.println();
  Serial.println("=== Soil ADC scan ===");
}

void loop() {
  for (int i = 0; i < N; i++) {
    int p = PINS[i];
    // چند نمونه میانگین
    long sum = 0;
    for (int k = 0; k < 8; k++) {
      sum += analogRead(p);
      delay(2);
    }
    int avg = (int)(sum / 8);
    // پین‌های احتمالی سنسور: نه 0 مطلق، نه همیشه 4095 شناور خشک
    // گزارش همه
    Serial.printf("GPIO%2d = %4d", p, avg);
    if (avg > 200 && avg < 4000) Serial.print("  << possible sensor");
    else if (avg >= 4000) Serial.print("  (open/float)");
    else Serial.print("  (low/gnd)");
    Serial.println();
  }
  Serial.println("---");
  delay(2000);
}
