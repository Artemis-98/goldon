# قوانین بردها — گلدون هوشمند

## معماری ارتباط (فعلی)

```
برد ۲ (پنل)  --وای فای HTTP-->  برد ۳ (مودم)  --UART-->  برد ۱ (حیاط)
```

### UART برد ۳ ↔ برد ۱

| Modem #3 | Yard #1  |
| -------- | -------- |
| GPIO8 TX | GPIO9 RX |
| GPIO9 RX | GPIO8 TX |
| GND      | GND      |

- حیاط: `YARD_LINK_UART 1` در `firmware/yard_api`
- مودم: `firmware/modem_bridge` — hostname `goldon-modem`
- پنل: HTTP به `goldon-modem.local` (وای فای)

### حالت جایگزین: حیاط مستقیم وای فای

اگر `YARD_LINK_UART 0` روی حیاط: پنل مستقیم به `goldon-yard` وصل می‌شود (بدون برد ۳).

---

## برد شماره ۱ — حیاط (Yard)

| فیلد      | مقدار                                 |
| --------- | ------------------------------------- |
| شماره     | **۱**                                 |
| محل نصب   | بیرون از خانه (حیاط)                  |
| ارتباط    | **UART به برد ۳** (یا وای فای مستقیم) |
| Hostname  | `goldon-yard` (فقط حالت وای فای)      |
| رمز OTA   | `ota2026` (فقط حالت وای فای)          |
| وای فای   | `X28-2.4G-94A50A`                     |
| سخت‌افزار | ESP32-S3-N8R2                         |
| USB نمونه | COM7                                  |

### قوانین آپلود

1. حالت UART: آپلود **USB**.
2. حالت وای فای: OTA با رمز `ota2026`.
3. FQBN:

```
esp32:esp32:esp32s3:CDCOnBoot=default,FlashSize=8M,PSRAM=disabled,PartitionScheme=default_8MB,USBMode=hwcdc,UploadSpeed=115200
```

### پین‌مپ نمونه

DHT=4 · WATER=18 · SOIL1=1 · SOIL2=2 · RELAY1=10 · RELAY2=11 · LINK TX/RX=8/9

---

## برد شماره ۲ — پنل کنترل (TFT Touch)

| فیلد      | مقدار                                  |
| --------- | -------------------------------------- |
| شماره     | **۲**                                  |
| نقش       | UI لمسی → فرمان به حیاط از طریق برد ۳  |
| سخت‌افزار | ESP32 Dev Module (`esp32:esp32:esp32`) |
| Hostname  | `goldon-panel`                         |
| IP نمونه  | `192.168.70.185`                       |
| وای فای   | `X28-2.4G-94A50A`                      |
| نمایشگر   | ILI9341 ۳.۲ اینچ ۲۴۰×۳۲۰               |
| لمس       | XPT2046                                |
| فریمور    | `firmware/panel_status`                |
| USB نمونه | COM14                                  |

پین‌های TFT: MOSI=23 SCLK=18 MISO=19 CS=5 DC=21 RST=22 TOUCH_CS=15

---

## برد شماره ۳ — مودم وای فای (Modem)

| فیلد      | مقدار                        |
| --------- | ---------------------------- |
| شماره     | **۳**                        |
| نقش       | پل وای فای بین پنل و حیاط    |
| سخت‌افزار | ESP32-S3 (هم‌خانوادهٔ برد ۱) |
| Hostname  | `goldon-modem`               |
| رمز OTA   | `ota2026`                    |
| وای فای   | `X28-2.4G-94A50A`            |
| فریمور    | `firmware/modem_bridge`      |
| USB نمونه | COM9                         |

آپلود USB یا OTA؛ پنل به `http://goldon-modem.local/status` وصل می‌شود.

---

## برد شماره ۴ — ESP32-C6 LCD 1.47 (اسپلش)

| فیلد      | مقدار                              |
| --------- | ---------------------------------- |
| شماره     | **۴**                              |
| نقش       | نمایش اسپلش HooshaAI روی LCD کوچک  |
| سخت‌افزار | Waveshare **ESP32-C6-LCD-1.47**    |
| نمایشگر   | ST7789 **۱۷۲×۳۲۰**، بک‌لایت GPIO22 |
| RGB       | WS2812 روی GPIO8                   |
| فریمور    | `firmware/c6_splash`               |
| USB نمونه | COM15 (Espressif USB-Serial/JTAG)  |

### پین‌های LCD

| سیگنال | GPIO |
| ------ | ---- |
| MOSI   | ۶    |
| SCLK   | ۷    |
| CS     | ۱۴   |
| DC     | ۱۵   |
| RST    | ۲۱   |
| BL     | ۲۲   |

### آپلود

```powershell
powershell -File scripts/upload_c6_splash.ps1 -Port COM15
```

FQBN:

```
esp32:esp32:esp32c6:CDCOnBoot=cdc,FlashSize=4M,PartitionScheme=default,UploadSpeed=921600
```

اسپلش: ۶۱ فریم JPEG (برش وسط از ویدیو ۲۴۰×۳۲۰) در LittleFS مسیر `/splash/f_XXX.jpg`.

صفحات UI (۱۷۲×۳۲۰): اسپلش → هواشناسی → اقلیم → خاک → پمپ۱ → پمپ۲
پیش‌نمایش: `docs/ui/c6-pages.html`
ناوبری: دکمهٔ **BOOT (GPIO9)** = صفحه بعد؛ در صفحات پمپ **دوبار BOOT** = روشن/خاموش رله.
