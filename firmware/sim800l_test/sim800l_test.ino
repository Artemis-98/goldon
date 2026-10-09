/*
 * SIM800L EVB test on courtyard ESP32-S3
 *
 * Map: SIM TXD->GPIO16  SIM RXD->GPIO17  SIM RST->GPIO18
 * Serial monitor: 115200
 */

#include <HardwareSerial.h>

static const int PIN_A = 16;
static const int PIN_B = 17;
static const int PIN_RST = 18;

HardwareSerial SimSerial(1);

static const uint32_t BAUD_CANDIDATES[] = {9600, 115200, 19200, 57600, 38400};
static const size_t BAUD_COUNT = sizeof(BAUD_CANDIDATES) / sizeof(BAUD_CANDIDATES[0]);

void drainMs(uint32_t ms) {
  const uint32_t start = millis();
  while (millis() - start < ms) {
    while (SimSerial.available()) Serial.write(SimSerial.read());
    delay(2);
  }
}

bool waitToken(uint32_t timeoutMs) {
  String buf;
  const uint32_t start = millis();
  while (millis() - start < timeoutMs) {
    while (SimSerial.available()) {
      char c = (char)SimSerial.read();
      Serial.write(c);
      buf += c;
      if (buf.indexOf("OK") >= 0) return true;
      if (buf.indexOf("ERROR") >= 0) return false;
    }
    delay(2);
  }
  if (buf.length()) {
    Serial.print("\n[raw] ");
    Serial.println(buf);
  }
  return false;
}

bool sendAt(const char *cmd, uint32_t timeoutMs = 4000) {
  Serial.printf("\n>>> %s\n", cmd);
  while (SimSerial.available()) SimSerial.read();
  SimSerial.print(cmd);
  SimSerial.print("\r\n");
  return waitToken(timeoutMs);
}

void setRst(bool level) {
  pinMode(PIN_RST, OUTPUT);
  digitalWrite(PIN_RST, level ? HIGH : LOW);
}

void pulseRstActiveLow() {
  Serial.println("RST pulse: HIGH -> LOW 250ms -> HIGH");
  setRst(true);
  delay(120);
  setRst(false);
  delay(250);
  setRst(true);
  delay(3500);
}

void pulseRstActiveHigh() {
  Serial.println("RST pulse: LOW -> HIGH 250ms -> LOW (idle LOW)");
  setRst(false);
  delay(120);
  setRst(true);
  delay(250);
  setRst(false);
  delay(3500);
}

bool probeAt(uint32_t baud) {
  Serial.printf("\n--- baud %lu ---\n", (unsigned long)baud);
  SimSerial.updateBaudRate(baud);
  delay(40);
  while (SimSerial.available()) SimSerial.read();
  for (int i = 0; i < 3; i++) {
    SimSerial.print("AT\r\n");
    if (waitToken(800)) return true;
  }
  return false;
}

bool scanBauds() {
  for (size_t i = 0; i < BAUD_COUNT; i++) {
    if (probeAt(BAUD_CANDIDATES[i])) return true;
  }
  return false;
}

void runAtSuite() {
  Serial.println("\n========== AT suite ==========");
  sendAt("AT");
  sendAt("ATE0");
  sendAt("ATI");
  sendAt("AT+GSV");
  sendAt("AT+CGSN");
  sendAt("AT+CCID", 5000);
  sendAt("AT+CPIN?", 5000);
  sendAt("AT+CSQ");
  sendAt("AT+CREG?");
  sendAt("AT+COPS?");
  sendAt("AT+CGATT?");
  sendAt("AT+CBC");
  Serial.println("\n========== suite done ==========");
}

bool tryMapping(int rxPin, int txPin, const char *label) {
  Serial.printf("\n##### UART map: %s  RX=%d TX=%d #####\n", label, rxPin, txPin);
  SimSerial.end();
  delay(20);
  SimSerial.begin(9600, SERIAL_8N1, rxPin, txPin);
  delay(20);

  pulseRstActiveLow();
  drainMs(800);
  if (scanBauds()) return true;

  Serial.println("No AT after active-LOW reset. Trying active-HIGH reset...");
  pulseRstActiveHigh();
  drainMs(800);
  if (scanBauds()) return true;

  return false;
}

void setup() {
  Serial.begin(115200);
  delay(1200);
  Serial.println();
  Serial.println("================================");
  Serial.println(" SIM800L test  ESP32-S3 yard");
  Serial.println(" GPIO16/17 UART  GPIO18 RST");
  Serial.println("================================");

  bool ok = tryMapping(PIN_A, PIN_B, "diagram TXD->16 RXD->17");
  if (!ok) {
    Serial.println("\nDiagram mapping failed. Swapping TX/RX...");
    ok = tryMapping(PIN_B, PIN_A, "SWAPPED RX=17 TX=16");
  }

  if (!ok) {
    Serial.println("\n*** NO AT RESPONSE ***");
    Serial.println("Check: SIM 5V powerbank + 1000uF, common GND,");
    Serial.println("NET LED blink, SIM inserted, TX/RX wires.");
    SimSerial.end();
    SimSerial.begin(9600, SERIAL_8N1, PIN_A, PIN_B);
    setRst(true);
  } else {
    runAtSuite();
  }

  Serial.println("Passthrough ON. Type AT in monitor.");
}

void loop() {
  while (Serial.available()) SimSerial.write(Serial.read());
  while (SimSerial.available()) Serial.write(SimSerial.read());
}
