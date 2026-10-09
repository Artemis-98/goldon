/*
 * گلدون هوشمند — برد ۲ (پنل TFT 240×320)
 * صفحات: SPLASH → WEATHER → CLIMATE → SOIL → PUMP1 → PUMP2
 */

#include <وای فای.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <TFT_eSPI.h>
#include <LittleFS.h>
#include <TJpg_Decoder.h>
#include <time.h>

// ---------- لینک حیاط: UART (اولویت) + HTTP fallback ----------
#define YARD_LINK_UART 0
#if YARD_LINK_UART
// Panel Serial2: RX=16 TX=17  ↔  Yard TX=8 RX=9
static const int PIN_YARD_RX = 16;
static const int PIN_YARD_TX = 17;
static const uint32_t YARD_UART_BAUD = 115200;
HardwareSerial YardLink(2);
#endif

// ---------- شبکه (هواشناسی + HTTP fallback) ----------
static const char *وای فای_SSID = "YOUR_WIFI_SSID";
static const char *وای فای_PASS = "YOUR_WIFI_PASSWORD";
static const char *YARD_BASES[] = {
  "http://192.168.70.193",       // برد ۳ مودم (IP فعلی)
  "http://goldon-modem.local",
  "http://goldon-yard.local",
};
static const int YARD_BASE_COUNT = 3;

// سیرجان
static const float SIRJAN_LAT = 29.4514f;
static const float SIRJAN_LON = 55.6809f;
static const char *WEATHER_CACHE = "/weather.json";
static const time_t WEATHER_TTL_SEC = 24L * 60L * 60L;
static const int FORECAST_DAYS = 5;  // امروز + ۴ روز بعد

// ---------- زمان‌بندی ----------
static const unsigned long POLL_MS = 12000;  // رفرش کمتر = لمس روان‌تر
static const unsigned long TOUCH_GUARD_MS = 900;  // بعد از لمس، شبکه نزن
static const unsigned long وای فای_RETRY_MS = 10000;
static const unsigned long TOUCH_DEBOUNCE_MS = 350;
// اسپلش سبک برای فریم‌ریت بالاتر روی ESP32 (۱۲ fps · JPEG فشرده)
static const int SPLASH_FRAME_COUNT = 61;
static const uint16_t SPLASH_TOUCH_THRESHOLD = 400;

// ---------- رنگ / چیدمان (صفحه هوا = طراحی HTML) ----------
static const uint16_t COL_BG = TFT_BLACK;
static const uint16_t COL_FG = TFT_WHITE;
static const uint16_t COL_DIM = TFT_DARKGREY;

// Weather palette (از panel-pages.html)
static const uint16_t WX_BG      = 0x1106;  // #102030
static const uint16_t WX_TODAY   = 0x1C9F;  // #1e90ff
static const uint16_t WX_METRICS = 0x1BDC;  // #187ae0
static const uint16_t WX_BADGE   = 0xFE8F;  // #ffd27a
static const uint16_t WX_CARD    = 0x1988;  // #1a3044
static const uint16_t WX_CARD_BD = 0x2A4C;  // #2d4a63
static const uint16_t WX_MUTE    = 0x7C95;  // #7d93a8
static const uint16_t WX_WD      = 0xC6BC;  // #c5d4e2
static const uint16_t WX_SLASH   = 0x6C12;  // #6d8296
static const uint16_t WX_RAIN_BG = 0x1A4D;  // #1a4a6e
static const uint16_t WX_RAIN_FG = 0x7E5F;  // #7ec8ff
static const uint16_t WX_DRY_BG  = 0x39A4;  // #3d3420
static const uint16_t WX_LINE    = 0x21AA;  // #2a3d52 approx
static const uint16_t WX_CITY    = 0xDEFB;  // #e8f4ff approx
static const uint16_t WX_HL      = 0xDEFB;  // #d6ecff
static const uint16_t WX_MLABEL  = 0xCE7D;  // #cfe6ff
static const uint16_t WX_MBORDER = 0x4D1E;  // #4aa0f0 approx

// Climate palette (صفحه ۳)
static const uint16_t CL_BG      = 0x10C4;  // #121820
static const uint16_t CL_LINE    = 0x29C9;  // #2a3848
static const uint16_t CL_TEMP_BG = 0x1947;  // #1e2a3a
static const uint16_t CL_TEMP_BD = 0x3AAA;  // #3a5570
static const uint16_t CL_TEMP_AC = 0xF449;  // #ff8a4c
static const uint16_t CL_TEMP_LB = 0xF591;  // #ffb08a
static const uint16_t CL_TEMP_BB = 0x3944;  // #3a2a20 badge bg
static const uint16_t CL_TEMP_VL = 0xF71A;  // #ffe0d0
static const uint16_t CL_HUM_BG  = 0x1966;  // #1a2c34
static const uint16_t CL_HUM_BD  = 0x330E;  // #356070
static const uint16_t CL_HUM_AC  = 0x4E3D;  // #4ec4e8
static const uint16_t CL_HUM_LB  = 0x8EDE;  // #8ad8f0
static const uint16_t CL_HUM_BB  = 0x1DA8;  // #1a3540
static const uint16_t CL_HUM_VL  = 0xD7BF;  // #d6f4ff

// Soil palette (صفحه ۴)
static const uint16_t SO_BG      = 0x1104;  // #142018
static const uint16_t SO_LINE    = 0x21E6;  // #2a3d30
static const uint16_t SO_CARD    = 0x1964;  // #1c2e24
static const uint16_t SO_CARD_BD = 0x3287;  // #33503f
static const uint16_t SO_AC_A    = 0x3E71;  // #3ecf8e
static const uint16_t SO_AC_B    = 0x6E6E;  // #6bcb77
static const uint16_t SO_NAME    = 0xC733;  // #c8e6d0
static const uint16_t SO_OK_BG   = 0x1A66;  // #1e4d35
static const uint16_t SO_OK_FG   = 0x3E71;  // #3ecf8e
static const uint16_t SO_DRY_BG  = 0x49C3;  // #4a3a1a
static const uint16_t SO_DRY_FG  = 0xFE8F;  // #ffd27a
static const uint16_t SO_WET_BG  = 0x1A0A;  // #1a3d55
static const uint16_t SO_WET_FG  = 0x7E5F;  // #7ec8ff
static const uint16_t SO_META    = 0x8D73;  // #8fad9a
static const uint16_t SO_META_S  = 0xD753;  // #d4eadc
static const uint16_t SO_BAR_BG  = 0x08C2;  // #0f1a14
static const uint16_t SO_BAR_BD  = 0x2A06;  // #2a4034
static const uint16_t SO_BAR_LB  = 0x6C6F;  // #6f8f7c

// Pump palette (صفحه ۵ و ۶)
static const uint16_t PU_BG      = 0x1882;  // #181410
static const uint16_t PU_LINE    = 0x3985;  // #3a3228
static const uint16_t PU1_LINE   = 0x2A0A;  // #2a4050
static const uint16_t PU2_LINE   = 0x2A07;  // #2a4038
static const uint16_t PU1_HERO   = 0x1D4A;  // #1e3a52
static const uint16_t PU1_HERO_B = 0x3B52;  // #3a6a90
static const uint16_t PU1_TXT    = 0x9E7F;  // #9fd0ff
static const uint16_t PU2_HERO   = 0x1DE6;  // #1e3d32
static const uint16_t PU2_HERO_B = 0x3BCB;  // #3a7a60
static const uint16_t PU2_TXT    = 0x8ED6;  // #8fdbb8
static const uint16_t PU_BLK     = 0x20E3;  // #241e18
static const uint16_t PU_BLK_BD  = 0x49E6;  // #4a3f32
static const uint16_t PU_LBL     = 0x8BCF;  // #8a7a68
static const uint16_t PU_VAL     = 0xF733;  // #f0e6d8
static const uint16_t PU1_NEXT   = 0x7E5F;  // #7ec8ff
static const uint16_t PU2_NEXT   = 0x5E73;  // #5ec8a0
static const uint16_t PU_CHIP    = 0x18C2;  // #1a1612
static const uint16_t PU_CHIP_BD = 0x3164;  // #332b22
static const uint16_t PU_CHIP_L  = 0x6B0A;  // #6e6052
static const uint16_t PU_CHIP_V  = 0xDE76;  // #d8cbb8
static const uint16_t PU1_BTN_B  = 0x5CF9;  // #5a9fd4
static const uint16_t PU2_BTN_B  = 0x4CD0;  // #4a9a78

static const int PUMP1_DUR_SEC = 30;  // استاپ خودکار اگر فراموش شد
static const int PUMP2_DUR_SEC = 30;

// ADC خاک: مقدار بالاتر = خشک‌تر
static const int SOIL_ADC_DRY = 3500;
static const int SOIL_ADC_WET = 1000;

static const int M = 4;
static const int G = 4;
static const int TILE_W = 114;
static const int NAV_H = 28;       // طراحی: ۲۸px
static const int NAV_HIT = 40;
static const int TODAY_H = 148;    // طراحی: ۱۴۸px
static const int DAYS_H = 144;     // طراحی: ۱۴۴px
static const int HDR_H = 30;
static const int ROW1_Y = 36;
static const int ROW1_H = 64;
static const int ROW2_Y = 104;
static const int ROW2_H = 64;
static const int ROW3_Y = 172;
static const int ROW3_H = 48;
static const int RELAY_Y = 228;
static const int BTN_W = 50;
static const int BTN_H = 28;

static const char *TOUCH_CAL_PATH = "/touch.cal";
static uint16_t touchCal[5] = {300, 3600, 300, 3600, 6};

enum Page : uint8_t {
  PAGE_SPLASH = 0,
  PAGE_WEATHER = 1,
  PAGE_CLIMATE = 2,
  PAGE_SOIL = 3,
  PAGE_PUMP1 = 4,
  PAGE_PUMP2 = 5,
};

enum SoilLevel : uint8_t { SOIL_LVL_DRY = 0, SOIL_LVL_OK = 1, SOIL_LVL_WET = 2 };

TFT_eSPI tft = TFT_eSPI();

struct YardStatus {
  bool ok = false;
  int soil1 = 0;
  int soil2 = 0;
  float temp = NAN;
  float hum = NAN;
  bool water = false;
  bool relay1 = false;
  bool relay2 = false;
  bool aiAuto = true;
  int rssi = 0;
  String ip;
  String err;
};

struct DayForecast {
  char date[11] = "";   // YYYY-MM-DD
  int code = -1;
  float tMax = NAN;
  float tMin = NAN;
  int precipProb = -1;
};

struct WeatherData {
  bool ok = false;
  float temp = NAN;
  float hum = NAN;
  float wind = NAN;
  float feels = NAN;
  float uv = NAN;
  int code = -1;
  time_t fetchedAt = 0;
  String err;
  DayForecast days[FORECAST_DAYS];
  int dayCount = 0;
};

YardStatus yard;
WeatherData weather;
Page page = PAGE_SPLASH;
unsigned long lastPollMs = 0;
unsigned long lastTouchMs = 0;
unsigned long splashStartMs = 0;
unsigned long lastWeatherCheckMs = 0;
int splashFrame = 0;

// وضعیت آبیاری دستی پمپ‌ها
unsigned long pump1RunUntil = 0;
unsigned long pump2RunUntil = 0;
time_t pump1LastRun = 0;
time_t pump2LastRun = 0;
bool pumpBusy = false;
int activeBase = 0;
bool touchDown = false;
bool fsOk = false;
bool timeOk = false;
bool weatherFetchedThisBoot = false;
String panelIp = "-";
String yardBase = YARD_BASES[0];

// forward decls
void enterPage(Page p);
void drawWeather();
void drawClimate();
void drawSoil();
void drawPump(int pumpNo);
bool ensureWeather(bool force = false);
bool fetchYard();
bool setRelay(int n, bool on);
bool toggleAiAuto();

// ============================================================
// helpers
// ============================================================
const char *wmoLabel(int code) {
  if (code == 0) return "Clear";
  if (code == 1) return "Mainly clear";
  if (code == 2) return "Partly cloudy";
  if (code == 3) return "Overcast";
  if (code == 45 || code == 48) return "Fog";
  if (code >= 51 && code <= 57) return "Drizzle";
  if (code >= 61 && code <= 67) return "Rain";
  if (code >= 71 && code <= 77) return "Snow";
  if (code >= 80 && code <= 82) return "Showers";
  if (code >= 95 && code <= 99) return "Thunder";
  if (code < 0) return "--";
  return "Clouds";
}

String fmtFloat(float v, const char *unit) {
  if (isnan(v)) return String("--");
  char buf[24];
  snprintf(buf, sizeof(buf), "%.0f%s", v, unit);
  return String(buf);
}

String fmtFloat1(float v, const char *unit) {
  if (isnan(v)) return String("--");
  char buf[24];
  snprintf(buf, sizeof(buf), "%.1f%s", v, unit);
  return String(buf);
}

// روز هفته کوتاه از YYYY-MM-DD
const char *weekdayShort(const char *ymd) {
  if (!ymd || strlen(ymd) < 10) return "---";
  struct tm t = {};
  if (sscanf(ymd, "%d-%d-%d", &t.tm_year, &t.tm_mon, &t.tm_mday) != 3) return "---";
  t.tm_year -= 1900;
  t.tm_mon -= 1;
  t.tm_isdst = -1;
  mktime(&t);
  static const char *names[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  return names[t.tm_wday];
}

String shortDate(const char *ymd) {
  if (!ymd || strlen(ymd) < 10) return String("--");
  // MM-DD
  return String(ymd).substring(5);
}

bool hitButton(uint16_t x, uint16_t y, int bx, int by, int bw, int bh) {
  return x >= bx && x < (bx + bw) && y >= by && y < (by + bh);
}

// ============================================================
// JPEG splash
// ============================================================
bool jpgDraw(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap) {
  if (y >= tft.height()) return false;
  tft.pushImage(x, y, w, h, bitmap);
  return true;
}

String splashPath(int n) {
  char buf[28];
  snprintf(buf, sizeof(buf), "/splash/f_%03d.jpg", n);
  return String(buf);
}

bool drawSplashFrame(int n) {
  if (!fsOk) return false;
  char path[28];
  snprintf(path, sizeof(path), "/splash/f_%03d.jpg", n);
  File f = LittleFS.open(path, "r");
  if (!f) return false;
  size_t sz = f.size();
  // بافر ثابت — سریع‌تر از دیکود مستقیم از FS
  static uint8_t jpgBuf[8192];
  if (sz == 0 || sz > sizeof(jpgBuf)) {
    f.close();
    return false;
  }
  size_t got = f.read(jpgBuf, sz);
  f.close();
  if (got != sz) return false;
  return TJpgDec.drawJpg(0, 0, jpgBuf, sz) == 0;
}

void showSplashFallback() {
  tft.fillScreen(0xCE79);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_BLACK);
  tft.drawString("splash missing", 120, 160, 2);
}

// ============================================================
// Nav bar: back | title | next
// ============================================================
void drawNavBar(const char *title, bool showBack, bool showNext) {
  tft.fillRect(0, 0, 240, NAV_H, COL_BG);
  tft.drawFastHLine(0, NAV_H - 1, 240, COL_FG);

  if (showBack) {
    tft.drawRect(4, 3, 22, 22, COL_FG);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(COL_FG, COL_BG);
    tft.drawString("<", 15, 14, 2);
  }
  if (showNext) {
    tft.drawRect(214, 3, 22, 22, COL_FG);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(COL_FG, COL_BG);
    tft.drawString(">", 225, 14, 2);
  }

  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(COL_FG, COL_BG);
  tft.drawString(title, 120, 14, 2);
}

void drawWeatherNav() {
  tft.fillRect(0, 0, 240, NAV_H, WX_BG);
  tft.drawFastHLine(0, NAV_H - 1, 240, WX_LINE);

  tft.drawRoundRect(6, 3, 22, 22, 3, WX_MUTE);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE, WX_BG);
  tft.drawString("<", 17, 14, 2);

  tft.drawRoundRect(212, 3, 22, 22, 3, WX_MUTE);
  tft.drawString(">", 223, 14, 2);

  tft.drawString("SIRJAN", 120, 14, 2);
}

void drawClimateNav() {
  tft.fillRect(0, 0, 240, NAV_H, CL_BG);
  tft.drawFastHLine(0, NAV_H - 1, 240, CL_LINE);

  tft.drawRoundRect(6, 3, 22, 22, 3, WX_MUTE);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE, CL_BG);
  tft.drawString("<", 17, 14, 2);

  tft.drawRoundRect(212, 3, 22, 22, 3, WX_MUTE);
  tft.drawString(">", 223, 14, 2);

  tft.drawString("CLIMATE", 120, 14, 2);
}

void drawSoilNav() {
  tft.fillRect(0, 0, 240, NAV_H, SO_BG);
  tft.drawFastHLine(0, NAV_H - 1, 240, SO_LINE);

  tft.drawRoundRect(6, 3, 22, 22, 3, WX_MUTE);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE, SO_BG);
  tft.drawString("<", 17, 14, 2);

  tft.drawRoundRect(212, 3, 22, 22, 3, WX_MUTE);
  tft.drawString(">", 223, 14, 2);

  tft.drawString("SOIL", 120, 14, 2);
}

void drawPumpNav(int pumpNo) {
  uint16_t bg = PU_BG;
  uint16_t line = (pumpNo == 1) ? PU1_LINE : PU2_LINE;
  tft.fillRect(0, 0, 240, NAV_H, bg);
  tft.drawFastHLine(0, NAV_H - 1, 240, line);

  tft.drawRoundRect(6, 3, 22, 22, 3, WX_MUTE);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE, bg);
  tft.drawString("<", 17, 14, 2);

  if (pumpNo == 1) {
    tft.drawRoundRect(212, 3, 22, 22, 3, WX_MUTE);
    tft.drawString(">", 223, 14, 2);
  } else {
    tft.drawRoundRect(212, 3, 22, 22, 3, 0x3186);
    tft.setTextColor(0x528A, bg);
    tft.drawString(">", 223, 14, 2);
    tft.setTextColor(TFT_WHITE, bg);
  }

  tft.drawString(pumpNo == 1 ? "PUMP 1" : "PUMP 2", 120, 14, 2);
}

bool navHitBack(uint16_t x, uint16_t y) {
  // فقط گوشه چپ دکمه ‹ — با «بعدی» هم‌پوشانی ندارد
  if (x >= 52) return false;
  return (y < 48) || (y > 278);
}

bool navHitNext(uint16_t x, uint16_t y) {
  // فقط گوشه راست دکمه ›
  if (x <= 188) return false;
  return (y < 48) || (y > 278);
}

void saveTouchCal() {
  if (!fsOk) return;
  File f = LittleFS.open(TOUCH_CAL_PATH, "w");
  if (!f) return;
  f.write((uint8_t *)touchCal, sizeof(touchCal));
  f.close();
  Serial.printf("Touch cal saved: %u %u %u %u %u\n",
                touchCal[0], touchCal[1], touchCal[2], touchCal[3], touchCal[4]);
}

bool loadTouchCal() {
  if (!fsOk || !LittleFS.exists(TOUCH_CAL_PATH)) return false;
  File f = LittleFS.open(TOUCH_CAL_PATH, "r");
  if (!f) return false;
  if (f.read((uint8_t *)touchCal, sizeof(touchCal)) != (int)sizeof(touchCal)) {
    f.close();
    return false;
  }
  f.close();

  // کالیبره قبلی بدون invert_y بود → لمس بالا به y≈۳۱۹ می‌رفت
  // بیت‌ها: rotate=1, invert_x=2, invert_y=4
  if ((touchCal[4] & 0x04) == 0) {
    touchCal[4] = (uint16_t)(touchCal[4] | 0x04);
    Serial.printf("Touch cal: enable invert_y -> flags=%u\n", touchCal[4]);
    tft.setTouch(touchCal);
    saveTouchCal();
  } else {
    tft.setTouch(touchCal);
  }
  Serial.printf("Touch cal loaded: %u %u %u %u %u\n",
                touchCal[0], touchCal[1], touchCal[2], touchCal[3], touchCal[4]);
  return true;
}

void runTouchCalibration() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("TOUCH CAL", 16, 20, 4);
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.drawString("Tap the corners", 16, 60, 2);
  tft.drawString("as shown", 16, 82, 2);
  delay(400);
  tft.calibrateTouch(touchCal, TFT_WHITE, TFT_BLACK, 20);
  tft.setTouch(touchCal);
  saveTouchCal();
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("OK", 120, 150, 4);
  delay(600);
}

void ensureTouchCal() {
  if (loadTouchCal()) return;
  runTouchCalibration();
}

// خواندن لمس مقاوم در برابر کالیبره بد
bool readTouchXY(uint16_t *x, uint16_t *y, uint16_t threshold) {
  if (tft.getTouch(x, y, threshold)) return true;

  uint16_t z = tft.getTouchRawZ();
  if (z < threshold) return false;

  uint16_t rx = 0, ry = 0;
  tft.getTouchRaw(&rx, &ry);
  *x = rx;
  *y = ry;
  tft.convertRawXY(x, y);
  if (*x >= 240) *x = 239;
  if (*y >= 320) *y = 319;
  return true;
}

// ============================================================
// Weather fetch / cache
// ============================================================
bool saveWeatherCache() {
  if (!fsOk || !weather.ok) return false;
  JsonDocument doc;
  doc["fetchedAt"] = (long)weather.fetchedAt;
  doc["temp"] = weather.temp;
  doc["hum"] = weather.hum;
  doc["wind"] = weather.wind;
  doc["feels"] = weather.feels;
  doc["uv"] = weather.uv;
  doc["code"] = weather.code;
  JsonArray days = doc["days"].to<JsonArray>();
  for (int i = 0; i < weather.dayCount; i++) {
    JsonObject d = days.add<JsonObject>();
    d["date"] = weather.days[i].date;
    d["code"] = weather.days[i].code;
    d["tMax"] = weather.days[i].tMax;
    d["tMin"] = weather.days[i].tMin;
    d["pp"] = weather.days[i].precipProb;
  }
  File f = LittleFS.open(WEATHER_CACHE, "w");
  if (!f) return false;
  serializeJson(doc, f);
  f.close();
  return true;
}

bool loadWeatherCache() {
  if (!fsOk || !LittleFS.exists(WEATHER_CACHE)) return false;
  File f = LittleFS.open(WEATHER_CACHE, "r");
  if (!f) return false;
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) return false;

  weather.fetchedAt = (time_t)(doc["fetchedAt"] | 0);
  weather.temp = doc["temp"] | NAN;
  weather.hum = doc["hum"] | NAN;
  weather.wind = doc["wind"] | NAN;
  weather.feels = doc["feels"] | NAN;
  weather.uv = doc["uv"] | NAN;
  weather.code = doc["code"] | -1;
  weather.dayCount = 0;
  JsonArray days = doc["days"].as<JsonArray>();
  for (JsonObject d : days) {
    if (weather.dayCount >= FORECAST_DAYS) break;
    DayForecast &df = weather.days[weather.dayCount++];
    const char *date = d["date"] | "";
    strncpy(df.date, date, sizeof(df.date) - 1);
    df.date[sizeof(df.date) - 1] = 0;
    df.code = d["code"] | -1;
    df.tMax = d["tMax"] | NAN;
    df.tMin = d["tMin"] | NAN;
    df.precipProb = d["pp"] | -1;
  }
  weather.ok = weather.dayCount > 0 || !isnan(weather.temp);
  weather.err = weather.ok ? "" : "empty cache";
  return weather.ok;
}

bool weatherCacheFresh() {
  if (!weather.ok || weather.fetchedAt <= 0) return false;
  if (timeOk) {
    time_t now = time(nullptr);
    if (now < 1700000000 || weather.fetchedAt < 1700000000) return false;
    return (now - weather.fetchedAt) < WEATHER_TTL_SEC;
  }
  // بدون NTP معتبر: فقط اگر همین بوت از API گرفته شده
  return weatherFetchedThisBoot
         && (millis() < (unsigned long)WEATHER_TTL_SEC * 1000UL);
}

bool fetchWeatherApi() {
  if (وای فای.status() != WL_CONNECTED) {
    weather.err = "no وای فای";
    return false;
  }

  String url =
    String("http://api.open-meteo.com/v1/forecast")
    + "?latitude=" + String(SIRJAN_LAT, 4)
    + "&longitude=" + String(SIRJAN_LON, 4)
    + "&current=temperature_2m,relative_humidity_2m,apparent_temperature,weather_code,wind_speed_10m"
    + "&daily=weather_code,temperature_2m_max,temperature_2m_min,precipitation_probability_max,uv_index_max"
    + "&timezone=Asia%2FTehran"
    + "&forecast_days=" + String(FORECAST_DAYS);

  HTTPClient http;
  http.setTimeout(8000);
  http.setConnectTimeout(5000);
  if (!http.begin(url)) {
    weather.err = "begin fail";
    return false;
  }
  int code = http.GET();
  if (code != 200) {
    weather.err = String("HTTP ") + code;
    Serial.printf("Weather HTTP %d\n", code);
    http.end();
    return false;
  }
  String body = http.getString();
  http.end();
  Serial.printf("Weather body %u bytes\n", (unsigned)body.length());

  JsonDocument doc;
  DeserializationError jerr = deserializeJson(doc, body);
  if (jerr) {
    weather.err = "bad json";
    Serial.printf("Weather JSON %s\n", jerr.c_str());
    return false;
  }

  weather.temp = doc["current"]["temperature_2m"] | NAN;
  weather.hum = doc["current"]["relative_humidity_2m"] | NAN;
  weather.wind = doc["current"]["wind_speed_10m"] | NAN;
  weather.feels = doc["current"]["apparent_temperature"] | NAN;
  weather.code = doc["current"]["weather_code"] | -1;

  weather.dayCount = 0;
  JsonArray dates = doc["daily"]["time"].as<JsonArray>();
  JsonArray codes = doc["daily"]["weather_code"].as<JsonArray>();
  JsonArray tmax = doc["daily"]["temperature_2m_max"].as<JsonArray>();
  JsonArray tmin = doc["daily"]["temperature_2m_min"].as<JsonArray>();
  JsonArray pp = doc["daily"]["precipitation_probability_max"].as<JsonArray>();
  JsonArray uvs = doc["daily"]["uv_index_max"].as<JsonArray>();

  weather.uv = (uvs.size() > 0) ? (uvs[0] | NAN) : NAN;

  for (size_t i = 0; i < dates.size() && weather.dayCount < FORECAST_DAYS; i++) {
    DayForecast &df = weather.days[weather.dayCount++];
    const char *date = dates[i] | "";
    strncpy(df.date, date, sizeof(df.date) - 1);
    df.date[sizeof(df.date) - 1] = 0;
    df.code = codes[i] | -1;
    df.tMax = tmax[i] | NAN;
    df.tMin = tmin[i] | NAN;
    df.precipProb = pp[i] | -1;
  }

  weather.fetchedAt = timeOk ? time(nullptr) : (time_t)(millis() / 1000UL);
  weather.ok = true;
  weather.err = "";
  weatherFetchedThisBoot = true;
  saveWeatherCache();
  Serial.printf("Weather OK T=%.1f code=%d days=%d\n", weather.temp, weather.code, weather.dayCount);
  return true;
}

bool ensureWeather(bool force) {
  if (!force && weatherCacheFresh()) return true;
  if (!force && weather.ok && وای فای.status() != WL_CONNECTED) return true;
  if (loadWeatherCache() && weatherCacheFresh() && !force) return true;
  return fetchWeatherApi();
}

void syncTimeIfNeeded() {
  if (timeOk || وای فای.status() != WL_CONNECTED) return;
  configTzTime("IRST-3:30", "pool.ntp.org", "time.google.com");
  for (int i = 0; i < 20; i++) {
    time_t now = time(nullptr);
    if (now > 1700000000) {
      timeOk = true;
      Serial.printf("NTP OK %ld\n", (long)now);
      return;
    }
    delay(200);
  }
}

// ============================================================
// Weather UI — مو به مو مطابق docs/ui/panel-pages.html
// نوار ۲۸ + امروز ۱۴۸ + روزها ۱۴۴ = ۳۲۰
// ============================================================
bool isRainy(int code, int precipProb) {
  if (precipProb >= 40) return true;
  if (code >= 51 && code <= 67) return true;
  if (code >= 80 && code <= 99) return true;
  return false;
}

String formatTodayDateLabel() {
  static const char *WD[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
  static const char *MO[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN",
                             "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
  time_t now = time(nullptr);
  if (timeOk && now > 1700000000) {
    struct tm t;
    localtime_r(&now, &t);
    char buf[24];
    snprintf(buf, sizeof(buf), "%s %d %s", WD[t.tm_wday], t.tm_mday, MO[t.tm_mon]);
    return String(buf);
  }
  if (weather.dayCount > 0 && strlen(weather.days[0].date) >= 10) {
    int y = 0, m = 0, d = 0;
    sscanf(weather.days[0].date, "%d-%d-%d", &y, &m, &d);
    const char *wd = weekdayShort(weather.days[0].date);
    if (m >= 1 && m <= 12) {
      char buf[24];
      snprintf(buf, sizeof(buf), "%s %d %s", wd, d, MO[m - 1]);
      return String(buf);
    }
  }
  return String("TODAY");
}

void drawWxBadge(int x, int y, bool rain, bool todayStyle) {
  const char *txt = rain ? "RAIN" : "DRY";
  uint16_t bg, fg;
  int w;
  if (todayStyle && !rain) {
    bg = WX_BADGE;
    fg = WX_BG;
    w = 30;
  } else if (rain) {
    bg = WX_RAIN_BG;
    fg = WX_RAIN_FG;
    w = 34;
  } else {
    bg = WX_DRY_BG;
    fg = WX_BADGE;
    w = 30;
  }
  tft.fillRoundRect(x, y, w, 16, 8, bg);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(fg, bg);
  tft.drawString(txt, x + w / 2, y + 8, 1);
}

void drawDayCard(int x, int y, const DayForecast &d) {
  const int w = 105;
  const int h = 49;
  tft.fillRoundRect(x, y, w, h, 6, WX_CARD);
  tft.drawRoundRect(x, y, w, h, 6, WX_CARD_BD);

  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(WX_WD, WX_CARD);
  tft.drawString(weekdayShort(d.date), x + 8, y + 5, 2);

  bool rain = isRainy(d.code, d.precipProb);
  int bw = rain ? 34 : 30;
  drawWxBadge(x + w - bw - 6, y + 5, rain, false);

  char temps[20];
  char hiBuf[8], loBuf[8];
  if (isnan(d.tMax)) strcpy(hiBuf, "--");
  else snprintf(hiBuf, sizeof(hiBuf), "%d", (int)lroundf(d.tMax));
  if (isnan(d.tMin)) strcpy(loBuf, "--");
  else snprintf(loBuf, sizeof(loBuf), "%d", (int)lroundf(d.tMin));
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(TFT_WHITE, WX_CARD);
  tft.drawString(hiBuf, x + 8, y + 26, 2);
  int tw = tft.textWidth(hiBuf, 2);
  tft.setTextColor(WX_SLASH, WX_CARD);
  tft.drawString("/", x + 8 + tw + 2, y + 26, 2);
  int sw = tft.textWidth("/", 2);
  tft.setTextColor(TFT_WHITE, WX_CARD);
  tft.drawString(loBuf, x + 8 + tw + 2 + sw + 2, y + 26, 2);
}

void drawWeather() {
  tft.fillScreen(WX_BG);
  drawWeatherNav();

  const int todayY = NAV_H;  // 28

  if (!weather.ok) {
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(WX_MUTE, WX_BG);
    tft.drawString(weather.err.length() ? weather.err.c_str() : "Loading...", 120, 160, 2);
    return;
  }

  // —— TODAY block ۱۴۸px ——
  tft.fillRect(0, todayY, 240, TODAY_H, WX_TODAY);

  bool todayRain = false;
  float tMax = NAN, tMin = NAN;
  if (weather.dayCount > 0) {
    todayRain = isRainy(weather.days[0].code, weather.days[0].precipProb);
    tMax = weather.days[0].tMax;
    tMin = weather.days[0].tMin;
  } else {
    todayRain = isRainy(weather.code, -1);
  }

  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(WX_CITY, WX_TODAY);
  tft.drawString(formatTodayDateLabel(), 12, todayY + 10, 1);

  int badgeW = todayRain ? 34 : 30;
  drawWxBadge(240 - 12 - badgeW, todayY + 10, todayRain, true);

  // main: big temp + condition — بدون عدد جعلی ۰
  char tempBuf[8];
  bool tempMissing = isnan(weather.temp);
  if (tempMissing) strcpy(tempBuf, "--");
  else snprintf(tempBuf, sizeof(tempBuf), "%d", (int)lroundf(weather.temp));
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(TFT_WHITE, WX_TODAY);
  tft.drawString(tempBuf, 12, todayY + 36, tempMissing ? 4 : 6);
  int tempW = tft.textWidth(tempBuf, tempMissing ? 4 : 6);
  tft.setTextColor(WX_CITY, WX_TODAY);
  if (!tempMissing) tft.drawString("o", 12 + tempW + 2, todayY + 42, 2);

  tft.setTextDatum(TR_DATUM);
  tft.setTextColor(TFT_WHITE, WX_TODAY);
  const char *cond = tempMissing ? "--" : wmoLabel(weather.code);
  tft.drawString(cond, 228, todayY + 48, 2);

  char hl[24];
  if (!isnan(tMax) && !isnan(tMin)) {
    snprintf(hl, sizeof(hl), "H %d  L %d", (int)lroundf(tMax), (int)lroundf(tMin));
  } else if (!tempMissing) {
    int t = (int)lroundf(weather.temp);
    snprintf(hl, sizeof(hl), "H %d  L %d",
             isnan(tMax) ? t : (int)lroundf(tMax),
             isnan(tMin) ? t : (int)lroundf(tMin));
  } else {
    strcpy(hl, "H --  L --");
  }
  tft.setTextColor(WX_HL, WX_TODAY);
  tft.drawString(hl, 228, todayY + 68, 1);

  // metrics bar ۴۴px پایین today
  const int metY = todayY + TODAY_H - 10 - 44;  // padding bottom 10
  const int metX = 12;
  const int metW = 216;
  const int metH = 44;
  tft.fillRoundRect(metX, metY, metW, metH, 6, WX_METRICS);

  const char *labs[4] = {"HUM", "WIND", "FEELS", "UV"};
  char vals[4][12];
  if (isnan(weather.hum)) strcpy(vals[0], "--");
  else snprintf(vals[0], sizeof(vals[0]), "%d%%", (int)lroundf(weather.hum));
  if (isnan(weather.wind)) strcpy(vals[1], "--");
  else snprintf(vals[1], sizeof(vals[1]), "%d", (int)lroundf(weather.wind));
  if (isnan(weather.feels)) strcpy(vals[2], "--");
  else snprintf(vals[2], sizeof(vals[2]), "%d", (int)lroundf(weather.feels));
  if (isnan(weather.uv)) strcpy(vals[3], "--");
  else snprintf(vals[3], sizeof(vals[3]), "%d", (int)lroundf(weather.uv));

  int cellW = metW / 4;
  for (int i = 0; i < 4; i++) {
    int cx = metX + i * cellW;
    if (i > 0) tft.drawFastVLine(cx, metY + 6, metH - 12, WX_MBORDER);
    tft.setTextDatum(TC_DATUM);
    tft.setTextColor(WX_MLABEL, WX_METRICS);
    tft.drawString(labs[i], cx + cellW / 2, metY + 6, 1);
    tft.setTextColor(TFT_WHITE, WX_METRICS);
    tft.drawString(vals[i], cx + cellW / 2, metY + 22, 2);
  }

  // —— NEXT DAYS ۱۴۴px ——
  const int daysY = NAV_H + TODAY_H;  // 176
  tft.fillRect(0, daysY, 240, DAYS_H, WX_BG);

  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(WX_MUTE, WX_BG);
  tft.drawString("NEXT DAYS", 10, daysY + 8, 1);

  // 2×2 grid — روزهای ۱..۴ (فردا به بعد)
  const int gridY = daysY + 8 + 16 + 6;
  const int gap = 6;
  const int cardW = 105;
  const int cardH = 49;
  int shown = 0;
  for (int i = 1; i < weather.dayCount && shown < 4; i++, shown++) {
    int col = shown % 2;
    int row = shown / 2;
    int x = 10 + col * (cardW + (240 - 20 - 2 * cardW));  // space-between: 10 + 105 + gap + 105 + 10
    // gap between cards horizontally: (240 - 20 - 210) = 10 → use 10px? design gap 6 with justify space-between
    // left pad 10, right pad 10, two cards 105 → remaining 10 → so gap = 10
    x = 10 + col * (cardW + 10);
    int y = gridY + row * (cardH + gap);
    drawDayCard(x, y, weather.days[i]);
  }
}

// ============================================================
// Climate UI — صفحه ۳ مو به مو مطابق panel-pages.html
// نوار ۲۸ + دو کارت ۱۳۳ + فاصله = ۳۲۰
// ============================================================
void drawClimateCard(int y, bool isTemp, float value) {
  const int x = 10;
  const int w = 220;
  const int h = 133;

  uint16_t bg = isTemp ? CL_TEMP_BG : CL_HUM_BG;
  uint16_t bd = isTemp ? CL_TEMP_BD : CL_HUM_BD;
  uint16_t ac = isTemp ? CL_TEMP_AC : CL_HUM_AC;
  uint16_t lb = isTemp ? CL_TEMP_LB : CL_HUM_LB;
  uint16_t bb = isTemp ? CL_TEMP_BB : CL_HUM_BB;
  uint16_t vl = isTemp ? CL_TEMP_VL : CL_HUM_VL;

  tft.fillRoundRect(x, y, w, h, 8, bg);
  tft.drawRoundRect(x, y, w, h, 8, bd);
  // نوار بالای کارت
  tft.fillRect(x + 1, y + 1, w - 2, 3, ac);

  // head
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(lb, bg);
  tft.drawString(isTemp ? "TEMP" : "HUMIDITY", x + 14, y + 14, 2);

  // badge DHT11
  int bx = x + w - 14 - 40;
  tft.fillRoundRect(bx, y + 12, 40, 16, 8, bb);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(lb, bb);
  tft.drawString("DHT11", bx + 20, y + 20, 1);

  // big value — فونت ۴ برای -- تا خالی دیده نشود
  char num[12];
  bool missing = isnan(value);
  if (missing) strcpy(num, "--");
  else snprintf(num, sizeof(num), "%d", (int)lroundf(value));

  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(vl, bg);
  tft.drawString(num, x + 14, y + 48, missing ? 4 : 6);
  int nw = tft.textWidth(num, missing ? 4 : 6);
  tft.setTextColor(lb, bg);
  tft.drawString(isTemp ? "C" : "%", x + 14 + nw + 4, y + 58, 2);

  // foot
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(lb, bg);
  tft.drawString("AIR", x + 14, y + h - 24, 1);
  tft.setTextDatum(TR_DATUM);
  if (!yard.ok) {
    tft.drawString("OFFLINE", x + w - 14, y + h - 24, 1);
  } else if (missing) {
    tft.drawString("NO DHT", x + w - 14, y + h - 24, 1);
  } else {
    tft.drawString(yard.water ? "WATER OK" : "NO WATER", x + w - 14, y + h - 24, 1);
  }
}

void drawClimate() {
  tft.fillScreen(CL_BG);
  drawClimateNav();

  const int y1 = NAV_H + 8;
  const int y2 = y1 + 133 + 8;

  drawClimateCard(y1, true, yard.temp);
  drawClimateCard(y2, false, yard.hum);
}

// ============================================================
// Soil UI — صفحه ۴ مو به مو مطابق panel-pages.html
// ============================================================
int soilAdcToPct(int adc) {
  int a = constrain(adc, SOIL_ADC_WET, SOIL_ADC_DRY);
  return map(a, SOIL_ADC_DRY, SOIL_ADC_WET, 0, 100);
}

SoilLevel soilLevelFromPct(int pct) {
  if (pct < 35) return SOIL_LVL_DRY;
  if (pct > 70) return SOIL_LVL_WET;
  return SOIL_LVL_OK;
}

void drawSoilCard(int y, int index, int adc) {
  const int x = 10;
  const int w = 220;
  const int h = 133;
  bool isA = (index == 0);
  bool linked = yard.ok;
  int pct = linked ? soilAdcToPct(adc) : -1;
  SoilLevel lvl = linked ? soilLevelFromPct(pct) : SOIL_LVL_OK;
  bool dryTone = linked && (lvl == SOIL_LVL_DRY);

  uint16_t accent = isA ? SO_AC_A : SO_AC_B;
  if (dryTone) accent = SO_DRY_FG;
  uint16_t barFill = accent;

  tft.fillRoundRect(x, y, w, h, 8, SO_CARD);
  tft.drawRoundRect(x, y, w, h, 8, SO_CARD_BD);
  tft.fillRect(x + 1, y + 1, w - 2, 3, accent);

  // head: name + status
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(SO_NAME, SO_CARD);
  tft.drawString(isA ? "SOIL 1" : "SOIL 2", x + 12, y + 12, 2);

  const char *stTxt = linked ? "OK" : "OFF";
  uint16_t stBg = linked ? SO_OK_BG : SO_DRY_BG;
  uint16_t stFg = linked ? SO_OK_FG : SO_DRY_FG;
  int stW = linked ? 28 : 30;
  if (linked && lvl == SOIL_LVL_DRY) {
    stTxt = "DRY";
    stBg = SO_DRY_BG;
    stFg = SO_DRY_FG;
    stW = 30;
  } else if (linked && lvl == SOIL_LVL_WET) {
    stTxt = "WET";
    stBg = SO_WET_BG;
    stFg = SO_WET_FG;
    stW = 30;
  }
  int bx = x + w - 12 - stW;
  tft.fillRoundRect(bx, y + 10, stW, 16, 8, stBg);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(stFg, stBg);
  tft.drawString(stTxt, bx + stW / 2, y + 18, 1);

  // mid: pct + meta — بدون درصد جعلی وقتی حیاط آفلاین است
  char num[8];
  if (!linked) strcpy(num, "--");
  else snprintf(num, sizeof(num), "%d", pct);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(TFT_WHITE, SO_CARD);
  tft.drawString(num, x + 12, y + 40, linked ? 6 : 4);
  int nw = tft.textWidth(num, linked ? 6 : 4);
  tft.setTextColor(SO_META, SO_CARD);
  if (linked) tft.drawString("%", x + 12 + nw + 2, y + 52, 2);

  tft.setTextDatum(TR_DATUM);
  tft.setTextColor(SO_META_S, SO_CARD);
  tft.drawString(isA ? "Plant A" : "Plant B", x + w - 12, y + 48, 2);
  tft.setTextColor(SO_META, SO_CARD);
  char adcBuf[16];
  if (!linked) strcpy(adcBuf, "NO LINK");
  else snprintf(adcBuf, sizeof(adcBuf), "ADC %d", adc);
  tft.drawString(adcBuf, x + w - 12, y + 66, 1);

  // bar
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(SO_BAR_LB, SO_CARD);
  tft.drawString("MOISTURE", x + 12, y + h - 36, 1);
  tft.setTextDatum(TR_DATUM);
  tft.drawString("DRY <-> WET", x + w - 12, y + h - 36, 1);

  const int barX = x + 12;
  const int barY = y + h - 20;
  const int barW = w - 24;
  const int barH = 8;
  tft.fillRoundRect(barX, barY, barW, barH, 4, SO_BAR_BG);
  tft.drawRoundRect(barX, barY, barW, barH, 4, SO_BAR_BD);
  if (linked) {
    int fillW = (barW * constrain(pct, 0, 100)) / 100;
    if (fillW > 2) {
      tft.fillRoundRect(barX + 1, barY + 1, fillW - 2, barH - 2, 3, barFill);
    }
  }
}

void drawSoil() {
  tft.fillScreen(SO_BG);
  drawSoilNav();

  const int y1 = NAV_H + 8;
  const int y2 = y1 + 133 + 8;

  drawSoilCard(y1, 0, yard.soil1);
  drawSoilCard(y2, 1, yard.soil2);
}

// ============================================================
// Pump UI — صفحه ۵ و ۶ مو به مو مطابق panel-pages.html
// نوار ۲۸ + بدن ۲۹۲
// ============================================================
String formatPumpTime(time_t t) {
  if (t <= 0) return String("--");
  if (!timeOk || t < 1700000000) return String("Recent");
  struct tm tmNow = {}, tmRun = {};
  time_t now = time(nullptr);
  localtime_r(&now, &tmNow);
  localtime_r(&t, &tmRun);
  char buf[24];
  if (tmNow.tm_yday == tmRun.tm_yday && tmNow.tm_year == tmRun.tm_year) {
    snprintf(buf, sizeof(buf), "Today %02d:%02d", tmRun.tm_hour, tmRun.tm_min);
  } else {
    static const char *WD[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    snprintf(buf, sizeof(buf), "%s %02d:%02d", WD[tmRun.tm_wday], tmRun.tm_hour, tmRun.tm_min);
  }
  return String(buf);
}

String formatNextPlaceholder(int pumpNo) {
  (void)pumpNo;
  return String("--");
}

bool pumpIsRunning(int pumpNo) {
  unsigned long until = (pumpNo == 1) ? pump1RunUntil : pump2RunUntil;
  return until != 0 && millis() < until;
}

int pumpBtnY() {
  // pad 10 + hero 58 + gap 8 + 48 + 6 + 48 + gap 8 + 42 + gap 8 = 236 از NAV
  return NAV_H + 10 + 58 + 8 + 48 + 6 + 48 + 8 + 42 + 8;
}

bool pumpWaterBtnHit(uint16_t x, uint16_t y) {
  int by = pumpBtnY();
  return hitButton(x, y, 12, by, 150, 38);
}

bool pumpAiBtnHit(uint16_t x, uint16_t y) {
  int by = pumpBtnY();
  return hitButton(x, y, 170, by, 58, 38);
}

void drawPump(int pumpNo) {
  bool p1 = (pumpNo == 1);
  uint16_t heroBg = p1 ? PU1_HERO : PU2_HERO;
  uint16_t heroBd = p1 ? PU1_HERO_B : PU2_HERO_B;
  uint16_t heroTx = p1 ? PU1_TXT : PU2_TXT;
  uint16_t nextCol = p1 ? PU1_NEXT : PU2_NEXT;
  uint16_t btnBd = p1 ? PU1_BTN_B : PU2_BTN_B;
  uint16_t btnBg = heroBg;
  int dur = p1 ? PUMP1_DUR_SEC : PUMP2_DUR_SEC;
  bool running = pumpIsRunning(pumpNo);
  time_t last = p1 ? pump1LastRun : pump2LastRun;
  bool aiOn = yard.aiAuto;

  tft.fillScreen(PU_BG);
  drawPumpNav(pumpNo);

  const int bodyX = 12;
  const int chipH = 42;
  int y = NAV_H + 10;

  // hero 58
  tft.fillRoundRect(bodyX, y, 216, 58, 8, heroBg);
  tft.drawRoundRect(bodyX, y, 216, 58, 8, heroBd);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(heroTx, heroBg);
  tft.drawString(p1 ? "PUMP 1" : "PUMP 2", bodyX + 12, y + 10, 1);
  tft.drawString(p1 ? "Plant A" : "Plant B", bodyX + 12, y + 30, 4);
  y += 58 + 8;

  // LAST RUN
  tft.fillRoundRect(bodyX, y, 216, 48, 8, PU_BLK);
  tft.drawRoundRect(bodyX, y, 216, 48, 8, PU_BLK_BD);
  tft.setTextColor(PU_LBL, PU_BLK);
  tft.drawString("LAST RUN", bodyX + 12, y + 8, 1);
  tft.setTextColor(PU_VAL, PU_BLK);
  String lastStr = (last > 0) ? formatPumpTime(last) : String("--");
  tft.drawString(lastStr, bodyX + 12, y + 26, 2);
  y += 48 + 6;

  // NEXT RUN
  tft.fillRoundRect(bodyX, y, 216, 48, 8, PU_BLK);
  tft.drawRoundRect(bodyX, y, 216, 48, 8, PU_BLK_BD);
  tft.setTextColor(PU_LBL, PU_BLK);
  tft.drawString("NEXT RUN", bodyX + 12, y + 8, 1);
  tft.setTextColor(nextCol, PU_BLK);
  tft.drawString(formatNextPlaceholder(pumpNo), bodyX + 12, y + 26, 2);
  y += 48 + 8;

  // chips DURATION / STATUS
  tft.fillRoundRect(bodyX, y, 104, chipH, 6, PU_CHIP);
  tft.drawRoundRect(bodyX, y, 104, chipH, 6, PU_CHIP_BD);
  tft.setTextColor(PU_CHIP_L, PU_CHIP);
  tft.drawString("DURATION", bodyX + 8, y + 8, 1);
  tft.setTextColor(PU_CHIP_V, PU_CHIP);
  char durBuf[16];
  snprintf(durBuf, sizeof(durBuf), "%d sec", dur);
  tft.drawString(durBuf, bodyX + 8, y + 22, 2);

  tft.fillRoundRect(bodyX + 112, y, 104, chipH, 6, PU_CHIP);
  tft.drawRoundRect(bodyX + 112, y, 104, chipH, 6, PU_CHIP_BD);
  tft.setTextColor(PU_CHIP_L, PU_CHIP);
  tft.drawString("STATUS", bodyX + 120, y + 8, 1);
  tft.setTextColor(running ? nextCol : PU_CHIP_V, PU_CHIP);
  tft.drawString(running ? "Running" : "Idle", bodyX + 120, y + 22, 2);
  y += chipH + 8;

  // WATER NOW (چپ) + AI (راست)
  const int waterW = 150;
  const int aiW = 58;
  const int gap = 8;
  tft.fillRoundRect(bodyX, y, waterW, 38, 8, btnBg);
  tft.drawRoundRect(bodyX, y, waterW, 38, 8, btnBd);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(heroTx, btnBg);
  tft.drawString(running ? "STOP" : "WATER NOW", bodyX + waterW / 2, y + 19, 2);

  int aiX = bodyX + waterW + gap;
  uint16_t aiBg = aiOn ? btnBg : tft.color565(26, 26, 26);
  uint16_t aiBd = aiOn ? (p1 ? tft.color565(122, 184, 232) : tft.color565(106, 186, 152))
                       : tft.color565(58, 58, 58);
  uint16_t aiTx = aiOn ? (p1 ? tft.color565(200, 232, 255) : tft.color565(200, 240, 220))
                       : tft.color565(102, 102, 102);
  tft.fillRoundRect(aiX, y, aiW, 38, 8, aiBg);
  tft.drawRoundRect(aiX, y, aiW, 38, 8, aiBd);
  tft.setTextColor(aiTx, aiBg);
  tft.drawString("AI", aiX + aiW / 2, y + 19, 2);
}

bool startPumpWater(int pumpNo) {
  // فقط یکی همزمان
  if (pumpIsRunning(1) || pumpIsRunning(2)) return false;
  int dur = (pumpNo == 1) ? PUMP1_DUR_SEC : PUMP2_DUR_SEC;
  if (!setRelay(pumpNo, true)) return false;
  unsigned long until = millis() + (unsigned long)dur * 1000UL;
  if (pumpNo == 1) {
    pump1RunUntil = until;
    pump1LastRun = timeOk ? time(nullptr) : (time_t)(millis() / 1000UL);
  } else {
    pump2RunUntil = until;
    pump2LastRun = timeOk ? time(nullptr) : (time_t)(millis() / 1000UL);
  }
  return true;
}

bool stopPumpWater(int pumpNo) {
  if (!pumpIsRunning(pumpNo)) return false;
  setRelay(pumpNo, false);
  if (pumpNo == 1) pump1RunUntil = 0;
  else pump2RunUntil = 0;
  return true;
}

void tickPumps() {
  if (pump1RunUntil && millis() >= pump1RunUntil) {
    setRelay(1, false);
    pump1RunUntil = 0;
    if (page == PAGE_PUMP1) drawPump(1);
  }
  if (pump2RunUntil && millis() >= pump2RunUntil) {
    setRelay(2, false);
    pump2RunUntil = 0;
    if (page == PAGE_PUMP2) drawPump(2);
  }
}

// ============================================================
// HOME UI (legacy helpers — unused page removed)
// ============================================================
void drawTileFrame(int x, int y, int w, int h) {
  tft.drawRect(x, y, w, h, COL_FG);
  tft.fillRect(x + 1, y + 1, w - 2, h - 2, COL_BG);
}

void drawTile(int x, int y, int w, int h, const char *label, const String &value) {
  drawTileFrame(x, y, w, h);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(COL_DIM, COL_BG);
  tft.drawString(label, x + 8, y + 6, 2);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(COL_FG, COL_BG);
  tft.drawString(value, x + w / 2, y + h / 2 + 8, 4);
}

void drawRelayRow(int y, const char *title, bool on) {
  const int x0 = M;
  const int w = 240 - 2 * M;
  const int h = 40;
  tft.drawRect(x0, y, w, h, COL_FG);
  tft.fillRect(x0 + 1, y + 1, w - 2, h - 2, COL_BG);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(COL_FG, COL_BG);
  tft.drawString(title, x0 + 8, y + 4, 2);
  tft.setTextColor(on ? COL_FG : COL_DIM, COL_BG);
  tft.drawString(on ? "ON" : "OFF", x0 + 8, y + 22, 2);

  int onX = x0 + w - BTN_W * 2 - 12;
  int offX = x0 + w - BTN_W - 6;
  int btnY = y + (h - BTN_H) / 2;
  if (on) {
    tft.fillRect(onX, btnY, BTN_W, BTN_H, COL_FG);
    tft.setTextColor(COL_BG, COL_FG);
  } else {
    tft.drawRect(onX, btnY, BTN_W, BTN_H, COL_FG);
    tft.setTextColor(COL_FG, COL_BG);
  }
  tft.setTextDatum(MC_DATUM);
  tft.drawString("ON", onX + BTN_W / 2, btnY + BTN_H / 2, 2);
  if (!on) {
    tft.fillRect(offX, btnY, BTN_W, BTN_H, COL_FG);
    tft.setTextColor(COL_BG, COL_FG);
  } else {
    tft.drawRect(offX, btnY, BTN_W, BTN_H, COL_FG);
    tft.setTextColor(COL_FG, COL_BG);
  }
  tft.drawString("OFF", offX + BTN_W / 2, btnY + BTN_H / 2, 2);
}

void getRelayBtnRects(int rowY, int &onX, int &offX, int &btnY) {
  const int x0 = M;
  const int w = 240 - 2 * M;
  const int h = 40;
  onX = x0 + w - BTN_W * 2 - 12;
  offX = x0 + w - BTN_W - 6;
  btnY = rowY + (h - BTN_H) / 2;
}

void drawHome() {
  const int L = M;
  const int R = M + TILE_W + G;
  // فضای nav در بالا
  tft.fillScreen(COL_BG);
  drawNavBar("GOLDON", true, false);

  String tempStr = yard.ok ? fmtFloat1(yard.temp, "C") : String("--");
  String humStr = yard.ok ? fmtFloat1(yard.hum, "%") : String("--");
  String s1 = yard.ok ? String(yard.soil1) : String("--");
  String s2 = yard.ok ? String(yard.soil2) : String("--");
  String water = yard.ok ? (yard.water ? "YES" : "NO") : String("--");
  String link = yard.ok ? (String(yard.rssi) + "dB")
                        : (وای فای.status() == WL_CONNECTED ? "WAIT" : "NO وای فای");

  // کاشی‌ها کمی پایین‌تر به‌خاطر nav
  const int y0 = NAV_H + 4;
  drawTile(L, y0, TILE_W, 56, "TEMP", tempStr);
  drawTile(R, y0, TILE_W, 56, "HUM", humStr);
  drawTile(L, y0 + 60, TILE_W, 56, "SOIL 1", s1);
  drawTile(R, y0 + 60, TILE_W, 56, "SOIL 2", s2);
  drawTile(L, y0 + 120, TILE_W, 44, "WATER", water);
  drawTile(R, y0 + 120, TILE_W, 44, "LINK", link);
  drawRelayRow(y0 + 172, "RELAY 1", yard.relay1);
  drawRelayRow(y0 + 216, "RELAY 2", yard.relay2);
}

// ============================================================
// Yard API
// ============================================================
bool parseStatusPayload(const String &payload) {
  JsonDocument doc;
  if (deserializeJson(doc, payload)) {
    yard.err = "bad json";
    return false;
  }
  yard.ok = doc["ok"] | false;
  yard.soil1 = doc["soil1"] | 0;
  yard.soil2 = doc["soil2"] | 0;
  yard.water = doc["water"] | false;
  yard.relay1 = doc["relay1"] | false;
  yard.relay2 = doc["relay2"] | false;
  yard.aiAuto = doc["ai_auto"] | true;
  yard.rssi = doc["rssi"] | 0;
  yard.ip = doc["ip"] | "";
  yard.temp = doc["temp"].isNull() ? NAN : doc["temp"].as<float>();
  yard.hum = doc["hum"].isNull() ? NAN : doc["hum"].as<float>();
  if (!yard.ok) {
    yard.err = "ok=false";
    return false;
  }
  yard.err = "";
  return true;
}

bool httpGet(const String &url, String &outBody) {
  وای فایClient client;
  HTTPClient http;
  http.setTimeout(900);
  http.setConnectTimeout(700);
  http.setReuse(false);
  if (!http.begin(client, url)) {
    yard.err = "begin fail";
    return false;
  }
  int code = http.GET();
  if (code != 200) {
    yard.err = String("HTTP ") + code;
    http.end();
    return false;
  }
  outBody = http.getString();
  http.end();
  return true;
}

#if YARD_LINK_UART
// یک فرمان بفرست، یک خط JSON بگیر
bool yardUartExchange(const String &cmd, String &outBody, unsigned long timeoutMs = 800) {
  while (YardLink.available()) (void)YardLink.read();
  YardLink.print(cmd);
  YardLink.print('\n');
  YardLink.flush();

  String line;
  unsigned long t0 = millis();
  while (millis() - t0 < timeoutMs) {
    while (YardLink.available()) {
      char c = (char)YardLink.read();
      if (c == '\n' || c == '\r') {
        if (line.length() > 0) {
          outBody = line;
          return true;
        }
      } else if (line.length() < 600) {
        line += c;
      } else {
        line = "";
      }
    }
    delay(1);
  }
  yard.err = "uart timeout";
  return false;
}
#endif

bool fetchYardHttp() {
  if (وای فای.status() != WL_CONNECTED) {
    yard.err = "no وای فای";
    return false;
  }
  String body;
  if (httpGet(String(YARD_BASES[0]) + "/status", body) && parseStatusPayload(body)) {
    activeBase = 0;
    yardBase = YARD_BASES[0];
    return true;
  }
  if (YARD_BASE_COUNT > 1 &&
      httpGet(String(YARD_BASES[1]) + "/status", body) && parseStatusPayload(body)) {
    activeBase = 1;
    yardBase = YARD_BASES[1];
    return true;
  }
  if (!yard.err.length()) yard.err = "no yard";
  return false;
}

bool fetchYard() {
  // اگر تازه لمس شده، شبکه/UART سنگین نرو
  if (millis() - lastTouchMs < TOUCH_GUARD_MS) return false;

#if YARD_LINK_UART
  String body;
  if (yardUartExchange("STATUS", body) && parseStatusPayload(body)) {
    yardBase = "uart";
    return true;
  }
  // fallback HTTP اگر حیاط وای فای داشت
#endif
  if (!fetchYardHttp()) {
    yard.ok = false;
    return false;
  }
  return true;
}

bool waitوای فای(unsigned long timeoutMs) {
  unsigned long t0 = millis();
  while (وای فای.status() != WL_CONNECTED && millis() - t0 < timeoutMs) {
    delay(100);
  }
  return وای فای.status() == WL_CONNECTED;
}

bool setRelay(int n, bool on) {
#if YARD_LINK_UART
  String body;
  String cmd = String("RELAY,") + n + "," + (on ? "1" : "0");
  if (yardUartExchange(cmd, body) && parseStatusPayload(body)) {
    yardBase = "uart";
    return true;
  }
#endif
  if (وای فای.status() != WL_CONNECTED) return false;
  String state = on ? "1" : "0";
  auto tryUrl = [&](const String &base) -> bool {
    String body;
    if (!httpGet(base + "/relay?n=" + String(n) + "&state=" + state, body)) return false;
    return parseStatusPayload(body);
  };
  if (tryUrl(yardBase) && yardBase != "uart") return true;
  for (int i = 0; i < YARD_BASE_COUNT; i++) {
    if (tryUrl(YARD_BASES[i])) {
      activeBase = i;
      yardBase = YARD_BASES[i];
      return true;
    }
  }
  return false;
}

bool toggleAiAuto() {
#if YARD_LINK_UART
  String body;
  if (yardUartExchange("AI,toggle", body) && parseStatusPayload(body)) {
    yardBase = "uart";
    return true;
  }
#endif
  if (وای فای.status() != WL_CONNECTED) return false;
  auto tryUrl = [&](const String &base) -> bool {
    String body;
    if (!httpGet(base + "/ai?state=toggle", body)) return false;
    return parseStatusPayload(body);
  };
  if (tryUrl(yardBase) && yardBase != "uart") return true;
  for (int i = 0; i < YARD_BASE_COUNT; i++) {
    if (tryUrl(YARD_BASES[i])) {
      activeBase = i;
      yardBase = YARD_BASES[i];
      return true;
    }
  }
  return false;
}

void connectوای فایBackground() {
  وای فای.persistent(false);
  وای فای.mode(وای فای_STA);
  وای فای.setHostname("goldon-panel");
  وای فای.begin(وای فای_SSID, وای فای_PASS);
}

// ============================================================
// Pages / touch
// ============================================================
void enterPage(Page p) {
  page = p;
  touchDown = false;
  if (p == PAGE_SPLASH) {
    splashFrame = 0;
    splashStartMs = millis();
    if (!drawSplashFrame(1)) showSplashFallback();
    else splashFrame = 1;
  } else if (p == PAGE_WEATHER) {
    tft.fillScreen(WX_BG);
    drawWeatherNav();
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(WX_MUTE, WX_BG);
    tft.drawString("Loading...", 120, 160, 2);
    syncTimeIfNeeded();
    ensureWeather(false);
    drawWeather();
  } else if (p == PAGE_CLIMATE) {
    drawClimate();
    lastPollMs = millis();  // نه صفر — وگرنه فوراً fetch لمس را می‌کشد
  } else if (p == PAGE_SOIL) {
    drawSoil();
    lastPollMs = millis();
  } else if (p == PAGE_PUMP1) {
    drawPump(1);
    lastPollMs = millis();
  } else if (p == PAGE_PUMP2) {
    drawPump(2);
    lastPollMs = millis();
  }
}

void handleTouchSplash(uint16_t, uint16_t) {
  Serial.println("Splash touch -> weather");
  enterPage(PAGE_WEATHER);
}

void handleTouchClimate(uint16_t x, uint16_t y) {
  Serial.printf("CL touch x=%u y=%u\n", x, y);
  if (navHitBack(x, y)) {
    enterPage(PAGE_WEATHER);
    return;
  }
  if (navHitNext(x, y)) {
    enterPage(PAGE_SOIL);
  }
}

void handleTouchSoil(uint16_t x, uint16_t y) {
  Serial.printf("SO touch x=%u y=%u\n", x, y);
  if (navHitBack(x, y)) {
    enterPage(PAGE_CLIMATE);
    return;
  }
  if (navHitNext(x, y)) {
    enterPage(PAGE_PUMP1);
  }
}

void handleTouchWeather(uint16_t x, uint16_t y) {
  Serial.printf("WX touch x=%u y=%u\n", x, y);
  if (navHitBack(x, y)) {
    enterPage(PAGE_SPLASH);
    return;
  }
  if (navHitNext(x, y)) {
    enterPage(PAGE_CLIMATE);
  }
}

void handleTouchPump(int pumpNo, uint16_t x, uint16_t y) {
  Serial.printf("PU%d touch x=%u y=%u\n", pumpNo, x, y);
  const int by = pumpBtnY();
  const uint16_t yMirror = (uint16_t)(319 - y);
  const bool inBtnRow =
      (y >= (uint16_t)(by - 6) && y <= (uint16_t)(by + 44)) ||
      (yMirror >= (uint16_t)(by - 6) && yMirror <= (uint16_t)(by + 44));

  // WATER / AI
  if (inBtnRow) {
    if (x >= 170) {
      if (toggleAiAuto()) {
        Serial.printf("AI auto -> %s\n", yard.aiAuto ? "ON" : "OFF");
      } else {
        Serial.printf("AI toggle FAIL err=%s\n", yard.err.c_str());
      }
      drawPump(pumpNo);
      return;
    }
    if (x >= 8 && x < 170) {
      if (pumpIsRunning(pumpNo)) {
        stopPumpWater(pumpNo);
        drawPump(pumpNo);
      } else if (startPumpWater(pumpNo)) {
        drawPump(pumpNo);
      } else {
        Serial.printf("PU%d water FAIL وای فای=%d err=%s\n",
                      pumpNo, وای فای.status(), yard.err.c_str());
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(TFT_RED, PU_BG);
        tft.drawString(yard.err.length() ? yard.err : "FAIL", 120, by - 14, 2);
      }
      return;
    }
  }

  // ناوبری فقط گوشه‌های ‹ › — نه نصف صفحه
  if (navHitBack(x, y)) {
    enterPage(pumpNo == 1 ? PAGE_SOIL : PAGE_PUMP1);
    return;
  }
  if (pumpNo == 1 && navHitNext(x, y)) {
    enterPage(PAGE_PUMP2);
  }
}

bool pollSplashTouch() {
  uint16_t z = tft.getTouchRawZ();
  if (z < 300) {
    touchDown = false;
    return false;
  }
  if (touchDown) return false;
  if (millis() - lastTouchMs < 250) return false;
  touchDown = true;
  lastTouchMs = millis();
  Serial.printf("Splash touch Z=%u\n", z);
  handleTouchSplash(0, 0);
  return true;
}

bool pollTouch(uint16_t threshold) {
  uint16_t x = 0, y = 0;
  bool pressed = readTouchXY(&x, &y, threshold);
  if (!pressed) {
    touchDown = false;
    return false;
  }
  if (touchDown) return false;
  if (millis() - lastTouchMs < TOUCH_DEBOUNCE_MS) return false;
  touchDown = true;
  lastTouchMs = millis();
  // بعد از لمس، fetch را عقب بینداز تا UI قفل نشود
  lastPollMs = millis();

  if (page == PAGE_WEATHER) handleTouchWeather(x, y);
  else if (page == PAGE_CLIMATE) handleTouchClimate(x, y);
  else if (page == PAGE_SOIL) handleTouchSoil(x, y);
  else if (page == PAGE_PUMP1) handleTouchPump(1, x, y);
  else if (page == PAGE_PUMP2) handleTouchPump(2, x, y);
  return true;
}

void handleTouch() {
  if (page == PAGE_SPLASH) {
    pollSplashTouch();
    return;
  }
  // آستانه پایین‌تر = لمس حساس‌تر روی climate/soil
  pollTouch(200);
}

void tickSplash() {
  if (!fsOk) return;

  // قبل و بعد از هر فریم — تاچ با SPI صفحه تداخل دارد
  if (pollSplashTouch()) return;

  splashFrame++;
  if (splashFrame > SPLASH_FRAME_COUNT) splashFrame = 1;
  if (!drawSplashFrame(splashFrame) && splashFrame == 1) showSplashFallback();

  pollSplashTouch();
}

// ============================================================
void setup() {
  Serial.begin(115200);
  delay(200);

  tft.init();
  tft.setRotation(0);
  tft.fillScreen(0xCE79);

  fsOk = LittleFS.begin(true);
  Serial.printf("LittleFS %s\n", fsOk ? "OK" : "FAIL");
  loadWeatherCache();

  // کالیبره لمس — اگر فایل نبود، روی صفحه نقاط را لمس کن
  ensureTouchCal();

  TJpgDec.setJpgScale(1);
  TJpgDec.setSwapBytes(true);
  TJpgDec.setCallback(jpgDraw);

#if YARD_LINK_UART
  YardLink.begin(YARD_UART_BAUD, SERIAL_8N1, PIN_YARD_RX, PIN_YARD_TX);
  Serial.printf("Yard UART %lu baud RX=%d TX=%d\n",
                (unsigned long)YARD_UART_BAUD, PIN_YARD_RX, PIN_YARD_TX);
  Serial.println("Wire: Panel16<-Yard8 , Panel17->Yard9 , GND");
#endif

  connectوای فایBackground();
  enterPage(PAGE_SPLASH);

#if YARD_LINK_UART
  // حیاط را بدون وابستگی به وای فای پنل بخوان
  delay(300);
  fetchYard();
  Serial.printf("Yard UART first fetch ok=%d err=%s\n",
                yard.ok ? 1 : 0, yard.err.c_str());
#endif
}

void loop() {
  if (وای فای.status() == WL_CONNECTED && panelIp == "-") {
    panelIp = وای فای.localIP().toString();
    Serial.printf("Panel وای فای OK IP=%s\n", panelIp.c_str());
#if !YARD_LINK_UART
    fetchYard();
#endif
  } else if (وای فای.status() != WL_CONNECTED) {
    static unsigned long lastRetry = 0;
    if (millis() - lastRetry > وای فای_RETRY_MS) {
      lastRetry = millis();
      وای فای.reconnect();
    }
  }

  handleTouch();
  tickPumps();

  // هر دقیقه چک کن؛ اگر TTL گذشت دوباره بگیر
  if (page != PAGE_SPLASH && millis() - lastWeatherCheckMs > 60000UL) {
    lastWeatherCheckMs = millis();
    if (وای فای.status() == WL_CONNECTED && !weatherCacheFresh()) {
      if (fetchWeatherApi() && page == PAGE_WEATHER) drawWeather();
    }
  }

  if (page == PAGE_SPLASH) {
    tickSplash();
  } else if (page == PAGE_CLIMATE || page == PAGE_SOIL ||
             page == PAGE_PUMP1 || page == PAGE_PUMP2) {
    // اگر کاربر در حال لمس است، رفرش را عقب بینداز
    if (millis() - lastTouchMs < TOUCH_GUARD_MS) {
      // هیچ
    } else if (millis() - lastPollMs >= POLL_MS) {
      lastPollMs = millis();
      handleTouch();
      int prevS1 = yard.soil1, prevS2 = yard.soil2;
      float prevT = yard.temp, prevH = yard.hum;
      bool prevOk = yard.ok;
      bool prevR1 = yard.relay1, prevR2 = yard.relay2;
      bool prevAi = yard.aiAuto;

      if (!fetchYard()) {
        // fail یا لمس‌گارد — صفحه را نکش
      } else {
        handleTouch();
        // آستانه بالا برای ADC — نویز خاک هر بار full redraw نسازد
        bool soilChanged =
            (abs(yard.soil1 - prevS1) > 100) || (abs(yard.soil2 - prevS2) > 100);
        bool climateChanged =
            (isnan(prevT) != isnan(yard.temp)) || (isnan(prevH) != isnan(yard.hum)) ||
            (!isnan(prevT) && !isnan(yard.temp) && fabsf(prevT - yard.temp) > 0.6f) ||
            (!isnan(prevH) && !isnan(yard.hum) && fabsf(prevH - yard.hum) > 1.0f);
        bool linkChanged = (yard.ok != prevOk);
        bool pumpChanged =
            (yard.relay1 != prevR1) || (yard.relay2 != prevR2) || (yard.aiAuto != prevAi);

        if (page == PAGE_CLIMATE && (climateChanged || linkChanged)) {
          drawClimate();
        } else if (page == PAGE_SOIL && (soilChanged || linkChanged)) {
          drawSoil();
        } else if ((page == PAGE_PUMP1 || page == PAGE_PUMP2) && pumpChanged) {
          drawPump(page == PAGE_PUMP1 ? 1 : 2);
        }
      }
      handleTouch();
    }
    if (page == PAGE_PUMP1 || page == PAGE_PUMP2) {
      if (pumpIsRunning(page == PAGE_PUMP1 ? 1 : 2)) {
        static unsigned long lastPumpUi = 0;
        if (millis() - lastPumpUi > 1000) {
          lastPumpUi = millis();
          drawPump(page == PAGE_PUMP1 ? 1 : 2);
        }
      }
    }
  }
}
