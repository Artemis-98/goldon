"""موتور چک: سنسور + هوا + AI/قوانین → آبیاری ایمن."""

from __future__ import annotations

from datetime import datetime, timedelta
from pathlib import Path
from typing import Any

from .ai import ai_advise, local_decide
from .config import enrich_status
from .safety import pulsed_water
from .store import Store
from .weather import fetch_sirjan
from .yard import YardClient


def _parse_iso(s: str | None) -> datetime | None:
    if not s:
        return None
    try:
        return datetime.fromisoformat(s)
    except Exception:
        return None


def manual_locked(store: Store, cfg: dict) -> bool:
    meta = store.get_meta()
    last = _parse_iso(meta.get("last_manual_at"))
    if not last:
        return False
    mins = float(cfg.get("manual_lock_minutes") or 30)
    return datetime.now().astimezone() - last < timedelta(minutes=mins)


def collect_snapshot(cfg: dict, yard: YardClient) -> dict[str, Any]:
    status = enrich_status(yard.status(), cfg)
    weather = fetch_sirjan(cfg["sirjan_lat"], cfg["sirjan_lon"])
    return {"yard": status, "weather": weather, "yard_base": yard.active}


def apply_actions(
    cfg: dict,
    yard: YardClient,
    store: Store,
    actions: list[dict],
    dry_run: bool = False,
) -> list[dict]:
    """پمپ‌ها یکی‌یکی؛ هرگز همزمان."""
    results = []
    # اطمینان از خاموشی قبل از شروع
    if not dry_run:
        try:
            yard.all_off()
        except Exception:
            pass

    for a in actions:
        entry = {"action": a, "dry_run": dry_run}
        res = pulsed_water(cfg, yard, store, a, dry_run=dry_run)
        entry["result"] = res
        results.append(entry)
        # فاصله اجباری بین دو پمپ
        if not dry_run and len(actions) > 1:
            import time

            gap = float((cfg.get("safety") or {}).get("gap_between_pumps_sec", 60))
            time.sleep(max(0.0, gap))
    return results


def run_check(
    cfg: dict,
    yard: YardClient,
    store: Store,
    *,
    use_ai: bool = True,
    execute: bool = True,
    force: bool = False,
    extra_photos: list[Path] | None = None,
) -> dict[str, Any]:
    snap = collect_snapshot(cfg, yard)
    plan = store.get_plan()

    if not force and manual_locked(store, cfg):
        store.log("check_skipped_manual_lock")
        return {
            "ok": True,
            "skipped": "manual_lock",
            "snapshot": snap,
            "plan": plan,
        }

    if use_ai:
        decision = ai_advise(cfg, snap["yard"], snap["weather"], plan, extra_photos, store)
    else:
        decision = local_decide(cfg, snap["yard"], snap["weather"], plan, store)

    new_plan = dict(plan)
    if decision.get("check_every_hours"):
        new_plan["check_every_hours"] = float(decision["check_every_hours"])
    if decision.get("plants"):
        plants = dict(new_plan.get("plants") or {})
        plants.update(decision["plants"])
        new_plan["plants"] = plants
    if decision.get("species_guess"):
        new_plan["species_guess"] = decision["species_guess"]
    new_plan["source"] = decision.get("source")
    new_plan["last_notes"] = decision.get("notes")
    store.save_plan(new_plan)

    actions = decision.get("actions") or []
    # پنل: اگر AI خاموش باشد آبیاری خودکار اجرا نشود (دستی CLI همچنان مجاز)
    yard_ai = snap["yard"].get("ai_auto", True)
    if yard_ai is False or yard_ai == 0 or yard_ai == "false":
        if actions:
            decision = dict(decision)
            decision.setdefault("notes", []).append("AI auto OFF (panel) — watering skipped")
            actions = []
            decision["actions"] = actions

    # حداکثر یک پمپ در هر چک اگر تنظیم شده
    if (cfg.get("safety") or {}).get("one_plant_per_check", True) and len(actions) > 1:
        # خشک‌ترین را اول آبیاری کن
        def moist_of(a: dict) -> float:
            plant = next(
                (p for p in (cfg.get("plants") or []) if p.get("id") == a.get("plant_id")),
                None,
            )
            if not plant:
                return 999.0
            return float(snap["yard"].get(f"{plant.get('soil_key')}_pct") or 999.0)

        actions = sorted(actions, key=moist_of)[:1]
        decision = dict(decision)
        decision["actions"] = actions
        decision.setdefault("notes", []).append("safety: only driest plant watered this cycle")

    results = []
    if execute and actions:
        results = apply_actions(cfg, yard, store, actions, dry_run=False)
    elif actions:
        results = apply_actions(cfg, yard, store, actions, dry_run=True)

    store.set_meta(
        last_check_at=datetime.now().astimezone().isoformat(timespec="seconds"),
        last_check_source=decision.get("source"),
    )
    store.log(
        "check_done",
        source=decision.get("source"),
        actions=len(actions),
        check_every_hours=new_plan.get("check_every_hours"),
    )

    return {
        "ok": True,
        "snapshot": snap,
        "decision": decision,
        "plan": new_plan,
        "results": results,
        "executed": bool(execute and actions),
    }
