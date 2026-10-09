# Goldon — Smart Planter Ecosystem

ESP32-based smart planter with soil sensing, pump safety logic, LVGL status panel, and an optional AI host for watering decisions.

## Architecture

| Part | Role |
|------|------|
| **Yard** (ESP32-S3) | Soil sensors, relays/pumps, local HTTP API |
| **Modem / Bridge** | Wi‑Fi HTTP ↔ UART link to the yard board |
| **Panel** (ESP32 + LVGL) | Splash, weather, climate, soil, pump status UI |
| **Host** (Python) | Scheduling, safety limits, optional OpenAI/GapGPT plans |

```
Host (Python) --HTTP--> Modem/Bridge --UART--> Yard (sensors + pumps)
                 \--HTTP/mDNS--> Panel (LVGL)
```

## Setup (short)

1. In firmware sketches, set `YOUR_WIFI_SSID`, `YOUR_WIFI_PASSWORD`, and `YOUR_OTA_PASSWORD`.
2. Flash the relevant sketch under `firmware/` (Arduino / PlatformIO as per folder).
3. Host:
   ```bash
   cd host
   python -m pip install -r requirements.txt
   copy .env.example .env   # then add your API key if using AI
   # edit config.yaml (plants, moisture thresholds, safety)
   python -m goldon.cli
   ```

## Safety highlights

- One plant per check, gap between pumps
- Burst + soak watering with daily on-time cap
- Optional tank-empty / moisture verify gates

## Docs

Schematics, board notes, and project report live under `docs/`.

## License

Personal / portfolio project. Ask before commercial reuse.
