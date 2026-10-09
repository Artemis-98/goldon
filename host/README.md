# مغز لپ‌تاپ — گلدون هوشمند

کنترل برد ۱ از لپ‌تاپ: خواندن سنسور، هواشناسی سیرجان، فرمان دستی، و حلقهٔ AI/قوانین برای آبیاری.

## ایمنی آبیاری

- دو پمپ **همزمان** روشن نمی‌شوند (لپ‌تاپ + برد).
- آبیاری **نبضی**: ۸ ثانیه روشن → ~۵۵ ثانیه نفوذ → تکرار (قابل تنظیم در `config.yaml` → `safety`).
- بعد از آبیاری ~۳ دقیقه صبر؛ اگر رطوبت خاک بالا نرفت → آن پمپ `fault` می‌شود تا دستی رفع شود:
  - `py -3 -m goldon.cli faults`
  - `py -3 -m goldon.cli clear-fault --pump 1`
- سقف روزانه روشن بودن هر پمپ، فاصله بین دو جلسه، و عدم آبیاری خاک خیس.
- روی برد: اگر رله بیش از ۱۵ ثانیه یکسره بماند، خودکار قطع می‌شود.
- `py -3 -m goldon.cli all-off` برای خاموشی اضطراری.

## نصب

```bash
cd host
python -m pip install -r requirements.txt
```

## فرمان‌های دستی

```bash
cd host
python -m goldon.cli status
python -m goldon.cli weather
python -m goldon.cli water --pump 1 --sec 45
python -m goldon.cli on --pump 1
python -m goldon.cli off --pump 1
python -m goldon.cli toggle --pump 2
```

بعد از هر فرمان دستی، تا ۳۰ دقیقه (قابل تنظیم در `config.yaml`) حلقهٔ خودکار آبیاری را قفل می‌کند تا با کار دستی تداخل نکند.

## چک خودکار (یک‌بار)

```bash
# قوانین محلی (بدون کلید API)
python -m goldon.cli check --local

# فقط تصمیم، بدون روشن کردن پمپ
python -m goldon.cli check --local --dry

# با AI — GapGPT (سازگار با OpenAI):
# در host/.env:
# OPENAI_API_KEY=sk-...
# base_url در config.yaml: https://api.gapgpt.app/v1
py -3 -m goldon.cli check --dry
```

AI در پاسخ می‌تواند `check_every_hours` بدهد (مثلاً ۴ یا ۲۴). این عدد در `data/plans/active_plan.json` ذخیره می‌شود.

## حلقهٔ مداوم

```bash
python -m goldon.cli run --local
```

هر N ساعت (طبق برنامه) سنسور + هوا را می‌گیرد و در صورت نیاز آبیاری می‌کند.

## فایل‌ها

| مسیر | نقش |
|------|-----|
| `config.yaml` | IP برد، گیاهان، آستانه‌ها |
| `plants/` | عکس گیاهان برای AI |
| `data/plans/active_plan.json` | برنامه فعال + بازه چک |
| `data/logs/events.jsonl` | لاگ رویدادها |

## تنظیم IP برد

اگر IP عوض شد، در `config.yaml`:

```yaml
yard_base: "http://192.168.70.186"
```

## نکته ایمنی

- اگر `water: true` در status یعنی سطح آب — فعلاً فقط گزارش می‌شود؛ می‌توانی بعداً شرط «بدون آب → آبیاری نکن» اضافه کنی.
- ADC خاک را با `soil_adc_dry` / `soil_adc_wet` کالیبره کن تا درصد درست شود.
