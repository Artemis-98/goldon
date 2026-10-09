"""کلاینت HTTP برد ۱ (حیاط)."""

from __future__ import annotations

from typing import Any

import requests


class YardClient:
    def __init__(self, base: str, fallbacks: list[str] | None = None, timeout: float = 5.0):
        self.bases = [base.rstrip("/")] + [b.rstrip("/") for b in (fallbacks or [])]
        self.timeout = timeout
        self.active = self.bases[0]

    def _get(self, path: str, params: dict | None = None) -> dict[str, Any]:
        last_err: Exception | None = None
        for base in self.bases:
            url = f"{base}{path}"
            try:
                r = requests.get(url, params=params, timeout=self.timeout)
                r.raise_for_status()
                data = r.json()
                self.active = base
                return data
            except Exception as e:
                last_err = e
                continue
        raise RuntimeError(f"yard unreachable: {last_err}")

    def status(self) -> dict[str, Any]:
        return self._get("/status")

    def relay(self, n: int, state: str | int) -> dict[str, Any]:
        return self._get("/relay", {"n": n, "state": str(state)})

    def set_ai_auto(self, state: str | int | bool = "toggle") -> dict[str, Any]:
        if isinstance(state, bool):
            state = 1 if state else 0
        return self._get("/ai", {"state": str(state)})

    def all_off(self) -> dict[str, Any]:
        self.relay(1, 0)
        return self.relay(2, 0)

    def water(self, n: int, seconds: float) -> dict[str, Any]:
        """ساده: یک تکه روشن (ترجیح با pulsed_water در safety)."""
        import time

        other = 2 if n == 1 else 1
        self.relay(other, 0)
        on = self.relay(n, 1)
        try:
            time.sleep(max(0.5, float(seconds)))
        finally:
            off = self.relay(n, 0)
            try:
                self.relay(other, 0)
            except Exception:
                pass
        return {"on": on, "off": off, "seconds": seconds, "pump": n}
