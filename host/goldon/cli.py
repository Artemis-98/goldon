#!/usr/bin/env python3
"""
گلدون هوشمند — کنترل از لپ‌تاپ

دستی:
  python -m goldon.cli status
  python -m goldon.cli water --pump 1 --sec 45
  python -m goldon.cli on --pump 1
  python -m goldon.cli off --pump 1

AI / خودکار:
  python -m goldon.cli check          # یک‌بار چک (+ آبیاری در صورت نیاز)
  python -m goldon.cli check --dry    # فقط تصمیم، بدون پمپ
  python -m goldon.cli check --local  # بدون API، فقط قوانین
  python -m goldon.cli advise --photo path.jpg
  python -m goldon.cli set-interval 4
  python -m goldon.cli run            # حلقه طبق برنامه
"""

from __future__ import annotations

import argparse
import json
import sys
import time
from pathlib import Path

from .config import data_dir, enrich_status, load_config
from .engine import collect_snapshot, run_check
from .store import Store
from .weather import fetch_sirjan
from .yard import YardClient


def _client(cfg: dict) -> YardClient:
    return YardClient(cfg["yard_base"], cfg.get("yard_fallback") or [])


def _store(cfg: dict) -> Store:
    return Store(data_dir(cfg))


def _print(data) -> None:
    print(json.dumps(data, ensure_ascii=False, indent=2, default=str))


def cmd_status(cfg, args):
    y = _client(cfg)
    st = enrich_status(y.status(), cfg)
    _print({"yard_base": y.active, "status": st})


def cmd_weather(cfg, args):
    w = fetch_sirjan(cfg["sirjan_lat"], cfg["sirjan_lon"])
    _print(w)


def cmd_on(cfg, args):
    y = _client(cfg)
    s = _store(cfg)
    s.mark_manual()
    other = 2 if args.pump == 1 else 1
    y.relay(other, 0)
    _print(y.relay(args.pump, 1))


def cmd_off(cfg, args):
    y = _client(cfg)
    s = _store(cfg)
    s.mark_manual()
    _print(y.relay(args.pump, 0))


def cmd_toggle(cfg, args):
    y = _client(cfg)
    s = _store(cfg)
    s.mark_manual()
    other = 2 if args.pump == 1 else 1
    y.relay(other, 0)
    _print(y.relay(args.pump, "toggle"))


def cmd_water(cfg, args):
    from .safety import pulsed_water

    y = _client(cfg)
    s = _store(cfg)
    s.mark_manual()
    action = {"pump": args.pump, "duration_sec": args.sec, "plant_id": "manual"}
    res = pulsed_water(cfg, y, s, action, dry_run=False)
    _print(res)


def cmd_all_off(cfg, args):
    y = _client(cfg)
    s = _store(cfg)
    s.mark_manual()
    _print(y.all_off())


def cmd_faults(cfg, args):
    s = _store(cfg)
    _print(s.get_meta().get("pump_faults") or {})


def cmd_clear_fault(cfg, args):
    from .safety import clear_pump_fault

    s = _store(cfg)
    _print(clear_pump_fault(s, args.pump))


def cmd_snapshot(cfg, args):
    y = _client(cfg)
    _print(collect_snapshot(cfg, y))


def cmd_check(cfg, args):
    y = _client(cfg)
    s = _store(cfg)
    photos = [Path(p) for p in (args.photo or [])]
    out = run_check(
        cfg,
        y,
        s,
        use_ai=not args.local,
        execute=not args.dry,
        force=args.force,
        extra_photos=photos or None,
    )
    _print(out)


def cmd_advise(cfg, args):
    """فقط مشاوره / به‌روزرسانی برنامه — بدون آبیاری."""
    y = _client(cfg)
    s = _store(cfg)
    photos = [Path(p) for p in (args.photo or [])]
    out = run_check(
        cfg,
        y,
        s,
        use_ai=not getattr(args, "local", False),
        execute=False,
        force=getattr(args, "force", False),
        extra_photos=photos or None,
    )
    _print(out)


def cmd_plan(cfg, args):
    s = _store(cfg)
    _print(s.get_plan())


def cmd_set_interval(cfg, args):
    s = _store(cfg)
    plan = s.get_plan()
    plan["check_every_hours"] = float(args.hours)
    plan["source"] = "manual"
    s.save_plan(plan)
    _print(plan)


def cmd_run(cfg, args):
    """حلقهٔ مداوم طبق check_every_hours داخل plan."""
    y = _client(cfg)
    s = _store(cfg)
    print("goldon runner started — Ctrl+C to stop", flush=True)
    while True:
        plan = s.get_plan()
        hours = float(plan.get("check_every_hours") or cfg.get("default_check_every_hours") or 4)
        try:
            out = run_check(
                cfg,
                y,
                s,
                use_ai=not args.local,
                execute=not args.dry,
                force=False,
            )
            src = (out.get("decision") or {}).get("source") or out.get("skipped")
            n = len((out.get("decision") or {}).get("actions") or [])
            print(f"[{time.strftime('%H:%M:%S')}] check ok source={src} actions={n} next={hours}h", flush=True)
            # اگر AI بازه را عوض کرد، از plan جدید بخوان
            plan = s.get_plan()
            hours = float(plan.get("check_every_hours") or hours)
        except Exception as e:
            print(f"[{time.strftime('%H:%M:%S')}] ERROR {e}", flush=True)
            s.log("runner_error", error=str(e))
        time.sleep(max(60.0, hours * 3600.0))


def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description="Goldon host brain — yard board control")
    p.add_argument("--config", default=None, help="path to config.yaml")
    sub = p.add_subparsers(dest="cmd", required=True)

    sub.add_parser("status", help="خواندن سنسورهای برد ۱")
    sub.add_parser("weather", help="هواشناسی سیرجان")
    sub.add_parser("snapshot", help="سنسور + هوا با هم")
    sub.add_parser("plan", help="نمایش برنامهٔ فعال")

    for name in ("on", "off", "toggle"):
        sp = sub.add_parser(name, help=f"رله {name}")
        sp.add_argument("--pump", type=int, required=True, choices=[1, 2])

    sp = sub.add_parser("water", help="آبیاری دستی نبضی + بررسی رطوبت")
    sp.add_argument("--pump", type=int, required=True, choices=[1, 2])
    sp.add_argument("--sec", type=float, default=40)

    sub.add_parser("all-off", help="خاموش کردن هر دو پمپ")
    sub.add_parser("faults", help="نمایش پمپ‌های fault شده")
    sp = sub.add_parser("clear-fault", help="پاک کردن fault پمپ")
    sp.add_argument("--pump", type=int, default=None, choices=[1, 2])

    sp = sub.add_parser("check", help="یک چرخه تصمیم + آبیاری")
    sp.add_argument("--dry", action="store_true", help="بدون روشن کردن پمپ")
    sp.add_argument("--local", action="store_true", help="بدون OpenAI")
    sp.add_argument("--force", action="store_true", help="نادیده گرفتن قفل دستی")
    sp.add_argument("--photo", action="append", default=[], help="عکس گیاه (قابل تکرار)")

    sp = sub.add_parser("advise", help="مشاوره AI / قوانین — بدون پمپ")
    sp.add_argument("--local", action="store_true", help="بدون OpenAI")
    sp.add_argument("--photo", action="append", default=[])
    sp.add_argument("--force", action="store_true")
    sp = sub.add_parser("set-interval", help="بازه چک (ساعت)")
    sp.add_argument("hours", type=float)

    sp = sub.add_parser("run", help="حلقه خودکار")
    sp.add_argument("--dry", action="store_true")
    sp.add_argument("--local", action="store_true")

    return p


def main(argv: list[str] | None = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)
    cfg = load_config(args.config)

    handlers = {
        "status": cmd_status,
        "weather": cmd_weather,
        "snapshot": cmd_snapshot,
        "plan": cmd_plan,
        "on": cmd_on,
        "off": cmd_off,
        "toggle": cmd_toggle,
        "water": cmd_water,
        "all-off": cmd_all_off,
        "faults": cmd_faults,
        "clear-fault": cmd_clear_fault,
        "check": cmd_check,
        "advise": cmd_advise,
        "set-interval": cmd_set_interval,
        "run": cmd_run,
    }
    handlers[args.cmd](cfg, args)
    return 0


if __name__ == "__main__":
    sys.exit(main())
