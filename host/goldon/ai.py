"""مشاور AI (اختیاری) + قوانین محلی."""

from __future__ import annotations

import base64
import json
import os
from pathlib import Path
from typing import Any


def local_decide(
    cfg: dict,
    yard: dict,
    weather: dict,
    plan: dict,
    store=None,
) -> dict[str, Any]:
    """بدون API: بر اساس آستانه خاک و باران سیرجان تصمیم بگیر."""
    actions: list[dict] = []
    notes: list[str] = []
    check_h = float(plan.get("check_every_hours") or cfg.get("default_check_every_hours") or 4)
    sc = cfg.get("safety") or {}
    max_session = float(sc.get("burst_on_sec", 8)) * int(sc.get("max_bursts_per_session", 5))

    plant_plans = plan.get("plants") or {}
    for p in cfg.get("plants") or []:
        pid = p["id"]
        pp = plant_plans.get(pid) or {}
        soil_key = p.get("soil_key", "soil1")
        pct_key = f"{soil_key}_pct"
        moist = yard.get(pct_key)
        threshold = float(pp.get("soil_moist_min_percent", p.get("soil_moist_min_percent", 35)))
        duration = min(
            float(pp.get("water_duration_sec", p.get("water_duration_sec", 40))),
            max_session,
        )
        skip_rain = bool(pp.get("skip_if_rain_today", True))
        pump = int(p.get("pump", 1))

        if store is not None:
            from .safety import pump_faulted

            if pump_faulted(store, pump):
                notes.append(f"{pid}: pump {pump} faulted — skip")
                continue

        if moist is None:
            notes.append(f"{pid}: soil unknown — skip")
            continue
        if skip_rain and weather.get("rain_today"):
            notes.append(f"{pid}: skip — rain expected/today in Sirjan")
            continue
        max_moist = float(sc.get("max_soil_percent_to_water", 68))
        if moist >= max_moist:
            notes.append(f"{pid}: already wet {moist:.0f}%")
            continue
        if moist < threshold:
            actions.append(
                {
                    "plant_id": pid,
                    "name": p.get("name"),
                    "pump": pump,
                    "duration_sec": duration,
                    "reason": f"soil {moist:.0f}% < {threshold:.0f}%",
                }
            )
        else:
            notes.append(f"{pid}: ok soil {moist:.0f}%")

    # فقط خشک‌ترین
    if len(actions) > 1 and sc.get("one_plant_per_check", True):
        actions.sort(
            key=lambda a: next(
                (
                    float(yard.get(f"{p.get('soil_key')}_pct") or 999)
                    for p in (cfg.get("plants") or [])
                    if p.get("id") == a.get("plant_id")
                ),
                999.0,
            )
        )
        dropped = actions[1:]
        actions = actions[:1]
        for d in dropped:
            notes.append(f"deferred {d.get('plant_id')} — one pump per cycle")

    return {
        "source": "local_rules",
        "check_every_hours": check_h,
        "actions": actions,
        "notes": notes,
        "plants": plant_plans,
    }


def _encode_image(path: Path) -> str | None:
    if not path.exists():
        return None
    raw = path.read_bytes()
    mime = "image/jpeg"
    if path.suffix.lower() == ".png":
        mime = "image/png"
    b64 = base64.standard_b64encode(raw).decode("ascii")
    return f"data:{mime};base64,{b64}"


def ai_advise(
    cfg: dict,
    yard: dict,
    weather: dict,
    plan: dict,
    photo_paths: list[Path] | None = None,
    store=None,
) -> dict[str, Any]:
    """اگر OPENAI_API_KEY باشد، از مدل بخواه برنامه بدهد؛ وگرنه local."""
    env_name = cfg.get("openai_api_key_env") or "OPENAI_API_KEY"
    key = os.environ.get(env_name, "").strip()
    if not key:
        return local_decide(cfg, yard, weather, plan, store)

    try:
        import requests
    except ImportError:
        return local_decide(cfg, yard, weather, plan, store)

    root = Path(cfg["_root"])
    content: list[dict] = [
        {
            "type": "text",
            "text": (
                "You are an irrigation advisor for a smart planter in Sirjan, Iran.\n"
                "HARD SAFETY RULES you must respect:\n"
                "- Never water two pumps in one response (actions length <= 1).\n"
                "- Prefer short total on-time; host will pulse water (burst+soak).\n"
                "- Prefer duration_sec between 24 and 40.\n"
                "- Skip if rain_today unless soil critically dry.\n"
                "- Do not suggest continuous watering.\n"
                "Return ONLY valid JSON with keys:\n"
                "check_every_hours (number),\n"
                "plants (object keyed by plant id with soil_moist_min_percent, water_duration_sec, skip_if_rain_today),\n"
                "actions (array of {plant_id, pump, duration_sec, reason} — max 1 item),\n"
                "notes (array of strings),\n"
                "species_guess (optional string).\n\n"
                f"YARD_SENSORS:\n{json.dumps(yard, ensure_ascii=False)}\n\n"
                f"SIRJAN_WEATHER:\n{json.dumps(weather, ensure_ascii=False)}\n\n"
                f"CURRENT_PLAN:\n{json.dumps(plan, ensure_ascii=False)}\n\n"
                f"PLANT_CONFIG:\n{json.dumps(cfg.get('plants'), ensure_ascii=False)}\n"
            ),
        }
    ]

    for p in photo_paths or []:
        uri = _encode_image(Path(p) if Path(p).is_absolute() else root / p)
        if uri:
            content.append({"type": "image_url", "image_url": {"url": uri}})

    # also attach configured plant photos
    for plant in cfg.get("plants") or []:
        rel = plant.get("photo")
        if not rel:
            continue
        path = root / rel
        uri = _encode_image(path)
        if uri:
            content.append(
                {
                    "type": "text",
                    "text": f"Photo for {plant.get('id')} ({plant.get('name')}):",
                }
            )
            content.append({"type": "image_url", "image_url": {"url": uri}})

    base = (cfg.get("openai_base_url") or "https://api.openai.com/v1").rstrip("/")
    model = cfg.get("openai_model") or "gpt-4o-mini"
    headers = {"Authorization": f"Bearer {key}", "Content-Type": "application/json"}
    body = {
        "model": model,
        "messages": [
            {"role": "system", "content": "Reply with JSON only. No markdown."},
            {"role": "user", "content": content},
        ],
        "temperature": 0.2,
    }
    try:
        r = requests.post(f"{base}/chat/completions", headers=headers, json=body, timeout=90)
        r.raise_for_status()
        text = r.json()["choices"][0]["message"]["content"]
    except Exception as exc:
        fallback = local_decide(cfg, yard, weather, plan, store)
        notes = list(fallback.get("notes") or [])
        notes.insert(0, f"AI unavailable ({type(exc).__name__}: {exc}); used local rules")
        fallback["notes"] = notes
        fallback["source"] = "local_rules_ai_fallback"
        return fallback

    text = text.strip()
    if text.startswith("```"):
        text = text.strip("`")
        if text.startswith("json"):
            text = text[4:].lstrip()
    parsed = json.loads(text)
    parsed["source"] = "ai"
    if "check_every_hours" not in parsed:
        parsed["check_every_hours"] = plan.get("check_every_hours") or cfg.get(
            "default_check_every_hours", 4
        )
    if "actions" not in parsed:
        parsed["actions"] = []
    return parsed
