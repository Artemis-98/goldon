"""قوانین ایمنی آبیاری."""

from __future__ import annotations

from datetime import datetime, timedelta
from typing import Any

from .config import enrich_status
from .store import Store
from .yard import YardClient


def _parse_iso(s: str | None) -> datetime | None:
    if not s:
        return None
    try:
        return datetime.fromisoformat(s)
    except Exception:
        return None


def safety_cfg(cfg: dict) -> dict:
    return cfg.get("safety") or {}


def pump_faulted(store: Store, pump: int) -> bool:
    faults = (store.get_meta().get("pump_faults") or {})
    return bool(faults.get(str(pump)))


def set_pump_fault(store: Store, pump: int, reason: str) -> None:
    meta = store.get_meta()
    faults = dict(meta.get("pump_faults") or {})
    faults[str(pump)] = {
        "reason": reason,
        "at": datetime.now().astimezone().isoformat(timespec="seconds"),
    }
    store.set_meta(pump_faults=faults)
    store.log("pump_fault", pump=pump, reason=reason)


def clear_pump_fault(store: Store, pump: int | None = None) -> dict:
    meta = store.get_meta()
    faults = dict(meta.get("pump_faults") or {})
    if pump is None:
        faults = {}
    else:
        faults.pop(str(pump), None)
    store.set_meta(pump_faults=faults)
    store.log("pump_fault_cleared", pump=pump)
    return faults


def daily_seconds(store: Store, pump: int) -> float:
    meta = store.get_meta()
    day = datetime.now().astimezone().date().isoformat()
    usage = meta.get("daily_water_sec") or {}
    if usage.get("day") != day:
        return 0.0
    return float((usage.get("pumps") or {}).get(str(pump), 0))


def add_daily_seconds(store: Store, pump: int, seconds: float) -> float:
    meta = store.get_meta()
    day = datetime.now().astimezone().date().isoformat()
    usage = meta.get("daily_water_sec") or {}
    if usage.get("day") != day:
        usage = {"day": day, "pumps": {}}
    pumps = dict(usage.get("pumps") or {})
    pumps[str(pump)] = float(pumps.get(str(pump), 0)) + float(seconds)
    usage["pumps"] = pumps
    store.set_meta(daily_water_sec=usage)
    return float(pumps[str(pump)])


def last_water_at(store: Store, pump: int) -> datetime | None:
    meta = store.get_meta()
    times = meta.get("last_water_at") or {}
    return _parse_iso(times.get(str(pump)))


def mark_watered(store: Store, pump: int) -> None:
    meta = store.get_meta()
    times = dict(meta.get("last_water_at") or {})
    times[str(pump)] = datetime.now().astimezone().isoformat(timespec="seconds")
    store.set_meta(last_water_at=times)


def preflight(
    cfg: dict,
    store: Store,
    yard_status: dict,
    action: dict,
) -> tuple[bool, str]:
    """قبل از آبیاری: آیا مجاز است؟"""
    sc = safety_cfg(cfg)
    pump = int(action["pump"])

    if pump_faulted(store, pump):
        return False, f"pump {pump} faulted — clear with: clear-fault --pump {pump}"

    # مخزن خالی: در سخت‌افزار فعلی water=true یعنی سنسور HIGH
    # اگر require_water_ok و منطق «خالی» را برعکس تعریف کرده باشی در config
    if sc.get("block_if_tank_empty"):
        # پیش‌فرض: water==false یعنی آب نیست (قابل تنظیم)
        empty_when = sc.get("tank_empty_when", False)
        if bool(yard_status.get("water")) == bool(empty_when):
            return False, "tank empty / water sensor blocks watering"

    max_daily = float(sc.get("max_daily_on_sec_per_pump", 180))
    used = daily_seconds(store, pump)
    need = float(action.get("duration_sec") or 0)
    if used + need > max_daily:
        return False, f"daily limit pump {pump}: {used:.0f}+{need:.0f}>{max_daily:.0f}s"

    cooldown = float(sc.get("min_minutes_between_waterings", 90))
    last = last_water_at(store, pump)
    if last and datetime.now().astimezone() - last < timedelta(minutes=cooldown):
        return False, f"cooldown pump {pump}: wait {cooldown:.0f} min between sessions"

    # اگر خاک خیلی خیس است آبیاری نکن
    plant = next((p for p in (cfg.get("plants") or []) if int(p.get("pump", 0)) == pump), None)
    if plant:
        key = f"{plant.get('soil_key', 'soil1')}_pct"
        moist = yard_status.get(key)
        max_moist = float(sc.get("max_soil_percent_to_water", 70))
        if moist is not None and moist >= max_moist:
            return False, f"soil already wet ({moist:.0f}% >= {max_moist:.0f}%)"

    return True, "ok"


def pulsed_water(
    cfg: dict,
    yard: YardClient,
    store: Store,
    action: dict,
    *,
    dry_run: bool = False,
) -> dict[str, Any]:
    """
    آبیاری نبضی:
    - فقط یک پمپ
    - burst روشن / pause نفوذ
    - بعد از جلسه، صبر برای تثبیت رطوبت؛ اگر خاک بالا نرفت → fault
    """
    import time

    sc = safety_cfg(cfg)
    pump = int(action["pump"])
    total_on = float(action.get("duration_sec") or 40)
    burst = float(sc.get("burst_on_sec", 8))
    pause = float(sc.get("soak_pause_sec", 50))
    max_burst = int(sc.get("max_bursts_per_session", 5))
    verify_wait = float(sc.get("moisture_verify_wait_sec", 180))
    min_delta = float(sc.get("min_soil_increase_percent", 3))

    plant = next((p for p in (cfg.get("plants") or []) if int(p.get("pump", 0)) == pump), None)
    soil_key = (plant or {}).get("soil_key", "soil1")
    pct_key = f"{soil_key}_pct"

    # همیشه پمپ دیگر را خاموش کن
    other = 2 if pump == 1 else 1
    st0 = enrich_status(yard.status(), cfg)
    ok, reason = preflight(cfg, store, st0, action)
    if not ok:
        store.log("water_blocked", pump=pump, reason=reason, plant=action.get("plant_id"))
        return {"ok": False, "blocked": reason, "pump": pump}

    before = st0.get(pct_key)
    bursts_done = 0
    on_accum = 0.0
    log_bursts: list[dict] = []

    if dry_run:
        store.log("water_dry_run", pump=pump, total_on=total_on, burst=burst, pause=pause)
        return {
            "ok": True,
            "dry_run": True,
            "pump": pump,
            "planned_on_sec": total_on,
            "burst_on_sec": burst,
            "soak_pause_sec": pause,
            "soil_before": before,
        }

    try:
        yard.relay(other, 0)  # ایمنی: پمپ دیگر خاموش
        while on_accum < total_on - 0.1 and bursts_done < max_burst:
            # دوباره چک کن هیچ پمپ دیگری روشن نباشد
            live = yard.status()
            if pump == 1 and live.get("relay2"):
                yard.relay(2, 0)
            if pump == 2 and live.get("relay1"):
                yard.relay(1, 0)

            slice_on = min(burst, total_on - on_accum)
            yard.relay(pump, 1)
            time.sleep(slice_on)
            yard.relay(pump, 0)
            on_accum += slice_on
            bursts_done += 1
            log_bursts.append({"on": slice_on, "n": bursts_done})
            store.log("water_burst", pump=pump, on_sec=slice_on, n=bursts_done)

            if on_accum < total_on - 0.1 and bursts_done < max_burst:
                time.sleep(pause)  # فرصت نفوذ

        add_daily_seconds(store, pump, on_accum)
        mark_watered(store, pump)

        # صبر برای نفوذ و خواندن مجدد
        time.sleep(verify_wait)
        after_st = enrich_status(yard.status(), cfg)
        after = after_st.get(pct_key)
        delta = None
        if before is not None and after is not None:
            delta = after - before
            if delta < min_delta:
                set_pump_fault(
                    store,
                    pump,
                    f"no moisture rise after water (Δ={delta:.1f}% < {min_delta}%)",
                )
                return {
                    "ok": False,
                    "pump": pump,
                    "fault": True,
                    "reason": "no_moisture_increase",
                    "soil_before": before,
                    "soil_after": after,
                    "delta": delta,
                    "on_sec": on_accum,
                    "bursts": log_bursts,
                }

        store.log(
            "water_done",
            pump=pump,
            on_sec=on_accum,
            bursts=bursts_done,
            soil_before=before,
            soil_after=after,
            delta=delta,
            plant=action.get("plant_id"),
            reason=action.get("reason"),
        )
        return {
            "ok": True,
            "pump": pump,
            "on_sec": on_accum,
            "bursts": log_bursts,
            "soil_before": before,
            "soil_after": after,
            "delta": delta,
        }
    finally:
        # خاموشی قطعی هر دو پمپ
        try:
            yard.relay(1, 0)
            yard.relay(2, 0)
        except Exception:
            pass
