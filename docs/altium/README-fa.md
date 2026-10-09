# گلدون هوشمند — بسته شماتیک برای Altium

منبع حقیقت: `docs/schematic-netlist.txt` و دیاگرام HTML موجود در پروژه.

## چرا این فایل‌ها؟
کنترل مستقیم GUI التیوم از این دستیار ممکن نیست. این بسته برای ترسیم/ایمپورت در Altium آماده شده.

## فایل‌ها
- `goldon-hoshmand-protel.net` — نت‌لیست فرمت Protel
- `BOM.csv` — فهرست قطعات
- `placement-guide.txt` — ترتیب چیدمان شیت‌ها و اتصالات مهم
- `ULN2003AN-pins.csv` — جدول ۱۶ پین برای Symbol Wizard
- `Create_ULN2003AN.pas` — اسکریپت ساخت سمبل در SchLib
- `Sheet4-ULN2003-guide.txt` — راهنمای شیت ۴ (نت‌ها و No ERC)

## ULN2003AN روی شیت (U3) — الان در Altium

ULN **سنسور نیست**؛ درایور رله است. روی **شیت ۴ — Actuators**.

### سریع‌ترین راه
1. روی شیت Schematic بزن `P` سپس `P` (Place Part)
2. در Components / Manufacturer Part Search بگرد: `ULN2003AN`
3. سمبل DIP-16 را Place کن → Designator = `U3`
4. نت‌ها را از `Sheet4-ULN2003-guide.txt` وصل کن

### اگر سمبل آماده نداری
1. `File → New → Library → Schematic Library`
2. یا با **Symbol Wizard** فایل `ULN2003AN-pins.csv` را ایمپورت کن  
   یا `File → Run Script → Create_ULN2003AN.pas`
3. Save کتابخانه → روی شیت Place → Part

### نت‌های الزامی U3
| پین | نام | نت |
|-----|-----|-----|
| 1 | 1B | `RLY1_IN_B` ← GPIO10 |
| 2 | 2B | `RLY2_IN_B` ← GPIO11 |
| 8 | GND | `GND` |
| 9 | COM | `+5V_BANK` |
| 15 | 2C | `RLY2_IN` → رله۲ IN |
| 16 | 1C | `RLY1_IN` → رله۱ IN |
| 3–7 و 10–14 | — | باز + **No ERC** |

## در Altium (پیشنهادی)
1. File → New → Project → PCB Project، اسم: `GoldonHoshmand`
2. Add New to Project → Schematic
3. قطعات را مطابق BOM بگذار (ماژول‌ها را با connector/sheet symbol هم می‌توانی مدل کنی)
4. نت‌ها را مطابق `schematic-netlist.txt` نام‌گذاری کن (`+5V_BANK`, `GND`, `+3V3`, `SIM_TX`, ...)
5. Design → Netlist → یا از فایل Protel برای چک اتصالات استفاده کن
6. ERC را اجرا کن؛ کانال‌های استفاده‌نشده ULN را با No ERC باز بگذار

## نکات حیاتی سیم‌کشی
- UART ضربدری: `SIM800L.TXD → ESP32.GPIO16` و `ESP32.GPIO17 → SIM800L.RXD`
- ورودی رله هرگز مستقیم به GPIO نرود؛ فقط از طریق ULN2003
- `C1` موازی روی `SIM 5VIN/GND` و نزدیک ماژول
- تغذیه پمپ ۳ ولت می‌تواند از GND کنترل جدا بماند (`GND_PUMP`)

## پین‌مپ خلاصه
| سیگنال | ESP32-S3 |
|--------|----------|
| SIM RX (از TXD ماژول) | GPIO16 |
| SIM TX (به RXD ماژول) | GPIO17 |
| SIM RST | GPIO18 |
| SOIL1 AO | GPIO1 |
| SOIL2 AO | GPIO2 |
| DHT11 DATA | GPIO4 |
| AP3216 SDA | GPIO5 |
| AP3216 SCL | GPIO9 |
| WATER S | GPIO7 |
| RELAY1 via ULN 1B | GPIO10 |
| RELAY2 via ULN 2B | GPIO11 |
