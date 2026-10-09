from __future__ import annotations

import os
from pathlib import Path

import yaml

ROOT = Path(__file__).resolve().parent.parent


def _load_dotenv(path: Path) -> None:
    """Load KEY=VALUE from .env into os.environ (does not override existing)."""
    if not path.is_file():
        return
    try:
        text = path.read_text(encoding="utf-8")
    except OSError:
        return
    for raw in text.splitlines():
        line = raw.strip()
        if not line or line.startswith("#") or "=" not in line:
            continue
        key, _, val = line.partition("=")
        key = key.strip()
        val = val.strip().strip('"').strip("'")
        if key and key not in os.environ:
            os.environ[key] = val


def load_config(path: str | Path | None = None) -> dict:
    _load_dotenv(ROOT / ".env")
    cfg_path = Path(path) if path else ROOT / "config.yaml"
    with open(cfg_path, encoding="utf-8") as f:
        cfg = yaml.safe_load(f) or {}
    cfg["_root"] = str(ROOT)
    cfg["_path"] = str(cfg_path)
    return cfg


def data_dir(cfg: dict) -> Path:
    d = Path(cfg.get("data_dir") or "data")
    if not d.is_absolute():
        d = ROOT / d
    d.mkdir(parents=True, exist_ok=True)
    (d / "logs").mkdir(exist_ok=True)
    (d / "plans").mkdir(exist_ok=True)
    return d


def soil_to_percent(adc: int | float | None, cfg: dict) -> float | None:
    if adc is None:
        return None
    dry = float(cfg.get("soil_adc_dry", 3200))
    wet = float(cfg.get("soil_adc_wet", 1200))
    if dry == wet:
        return None
    # خشک = عدد بالا → درصد پایین
    pct = (dry - float(adc)) / (dry - wet) * 100.0
    return max(0.0, min(100.0, pct))


def enrich_status(status: dict, cfg: dict) -> dict:
    out = dict(status)
    out["soil1_pct"] = soil_to_percent(status.get("soil1"), cfg)
    out["soil2_pct"] = soil_to_percent(status.get("soil2"), cfg)
    return out
