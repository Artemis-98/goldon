"""ذخیره برنامه، لاگ، قفل دستی."""

from __future__ import annotations

import json
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


def _now_iso() -> str:
    return datetime.now(timezone.utc).astimezone().isoformat(timespec="seconds")


class Store:
    def __init__(self, data_dir: Path):
        self.root = data_dir
        self.plan_path = data_dir / "plans" / "active_plan.json"
        self.meta_path = data_dir / "meta.json"
        self.log_path = data_dir / "logs" / "events.jsonl"

    def _read_json(self, path: Path, default: Any) -> Any:
        if not path.exists():
            return default
        with open(path, encoding="utf-8") as f:
            return json.load(f)

    def _write_json(self, path: Path, data: Any) -> None:
        path.parent.mkdir(parents=True, exist_ok=True)
        with open(path, "w", encoding="utf-8") as f:
            json.dump(data, f, ensure_ascii=False, indent=2)

    def log(self, event: str, **payload: Any) -> None:
        row = {"ts": _now_iso(), "event": event, **payload}
        with open(self.log_path, "a", encoding="utf-8") as f:
            f.write(json.dumps(row, ensure_ascii=False) + "\n")

    def get_meta(self) -> dict:
        return self._read_json(self.meta_path, {})

    def set_meta(self, **kwargs: Any) -> dict:
        meta = self.get_meta()
        meta.update(kwargs)
        meta["updated_at"] = _now_iso()
        self._write_json(self.meta_path, meta)
        return meta

    def mark_manual(self) -> None:
        self.set_meta(last_manual_at=_now_iso())
        self.log("manual_lock")

    def get_plan(self) -> dict:
        return self._read_json(
            self.plan_path,
            {
                "check_every_hours": 4,
                "plants": {},
                "source": "default",
                "updated_at": None,
            },
        )

    def save_plan(self, plan: dict) -> dict:
        plan = dict(plan)
        plan["updated_at"] = _now_iso()
        self._write_json(self.plan_path, plan)
        self.log("plan_saved", plan=plan)
        return plan
