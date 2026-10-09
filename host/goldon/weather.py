"""هواشناسی سیرجان — Open-Meteo."""

from __future__ import annotations

from typing import Any

import requests


def fetch_sirjan(lat: float, lon: float, timeout: float = 8.0) -> dict[str, Any]:
    url = "https://api.open-meteo.com/v1/forecast"
    params = {
        "latitude": lat,
        "longitude": lon,
        "current": "temperature_2m,relative_humidity_2m,weather_code,precipitation,wind_speed_10m",
        "daily": "weather_code,temperature_2m_max,temperature_2m_min,precipitation_probability_max,precipitation_sum",
        "timezone": "Asia/Tehran",
        "forecast_days": 3,
    }
    r = requests.get(url, params=params, timeout=timeout)
    r.raise_for_status()
    data = r.json()
    cur = data.get("current") or {}
    daily = data.get("daily") or {}
    rain_today = False
    try:
        rain_today = float((daily.get("precipitation_sum") or [0])[0] or 0) > 0.2
        code = int((daily.get("weather_code") or [-1])[0])
        if code >= 51:
            rain_today = True
    except Exception:
        pass

    return {
        "ok": True,
        "source": "open-meteo",
        "place": "Sirjan",
        "temp": cur.get("temperature_2m"),
        "hum": cur.get("relative_humidity_2m"),
        "wind": cur.get("wind_speed_10m"),
        "weather_code": cur.get("weather_code"),
        "precip_now": cur.get("precipitation"),
        "rain_today": rain_today,
        "daily": {
            "dates": daily.get("time"),
            "tmax": daily.get("temperature_2m_max"),
            "tmin": daily.get("temperature_2m_min"),
            "precip_prob": daily.get("precipitation_probability_max"),
            "precip_sum": daily.get("precipitation_sum"),
            "codes": daily.get("weather_code"),
        },
    }
