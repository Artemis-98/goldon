/*
 * تشخیص رله ۲ — ESP32-S3
 * رله ۱ (GPIO10) کار می‌کند؛ رله ۲ (GPIO11) نه.
 *
 * مسیر درست رله ۲:
 *   GPIO11 → ULN پین ۲ (2B) → ULN پین ۱۵ (2C) → IN رله ۲
 * IN رله را به 2B وصل نکن.
 */

static const int PIN_R1 = 10;
static const int PIN_R2 = 11;

void pulse(int pin, const char *name, bool activeHigh) {
  Serial.printf("%s %s ... ", name, activeHigh ? "HIGH" : "LOW(invert)");
  digitalWrite(pin, activeHigh ? HIGH : LOW);
  delay(1200);
  digitalWrite(pin, activeHigh ? LOW : HIGH);
  Serial.println("off");
  delay(800);
}

void setup() {
  Serial.begin(115200);
  delay(800);
  pinMode(PIN_R1, OUTPUT);
  pinMode(PIN_R2, OUTPUT);
  digitalWrite(PIN_R1, LOW);
  digitalWrite(PIN_R2, LOW);

  Serial.println();
  Serial.println("=== تشخيص رله 2 ===");
  Serial.println("اگر رله2 كليك نكرد: IN را از 2B جدا كن و به 2C (پين 15) بزن");
}

void loop() {
  Serial.println("--- رله1 GPIO10 (بايد كليك كند) ---");
  pulse(PIN_R1, "R1", true);

  Serial.println("--- رله2 GPIO11 حالت عادي HIGH ---");
  pulse(PIN_R2, "R2", true);

  Serial.println("--- رله2 GPIO11 حالت معكوس LOW ---");
  pulse(PIN_R2, "R2inv", false);

  Serial.println("--- هر دو با هم ---");
  digitalWrite(PIN_R1, HIGH);
  digitalWrite(PIN_R2, HIGH);
  delay(1200);
  digitalWrite(PIN_R1, LOW);
  digitalWrite(PIN_R2, LOW);
  delay(1500);
}
