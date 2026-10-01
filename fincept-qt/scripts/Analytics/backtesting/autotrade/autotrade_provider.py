#!/usr/bin/env python3
"""
Autotrade backtesting provider (root-1729 fork).

Thin client for the autotrade research service (http://autotrade.lan/research), which
backtests the TQQQ/SQQQ rotation models with the live engine's own code. Signals come
from ^NDX and trades go into TQQQ/SQQQ, so the tab's symbol selection is ignored.

    python autotrade_provider.py <command> '<json args>'

Prints {"success": bool, "data": ..., "error": ...} on stdout like the other providers.
Standard library only.

Service address, first match wins: FINCEPT_AUTOTRADE_URL, the terminal's
autotrade/api_url setting (macOS preferences), http://localhost:8000.
"""

import json
import os
import plistlib
import sys
import urllib.error
import urllib.request
from pathlib import Path

TIMEOUT_SECONDS = 1800  # walk-forward with many trials can take a few minutes


def _log(msg: str) -> None:
    print(f"[autotrade] {msg}", file=sys.stderr)


def base_url() -> str:
    url = os.environ.get("FINCEPT_AUTOTRADE_URL", "")
    if not url:
        plist = Path.home() / "Library/Preferences/com.fincept.FinceptTerminal.plist"
        try:
            with open(plist, "rb") as f:
                url = plistlib.load(f).get("autotrade.api_url", "")
        except (OSError, plistlib.InvalidFileException):
            pass
    return (url or "http://localhost:8000").rstrip("/") + "/research"


def call(method: str, path: str, body: dict = None):
    url = base_url() + path
    data = json.dumps(body).encode() if body is not None else None
    req = urllib.request.Request(url, data=data, method=method, headers={"Content-Type": "application/json"})
    try:
        with urllib.request.urlopen(req, timeout=TIMEOUT_SECONDS) as r:
            return json.loads(r.read().decode())
    except urllib.error.HTTPError as e:
        try:
            detail = json.loads(e.read().decode()).get("detail", "")
        except Exception:
            detail = ""
        raise RuntimeError(detail or f"research service returned HTTP {e.code}")
    except urllib.error.URLError as e:
        raise RuntimeError(f"cannot reach the autotrade research service at {url}: {e.reason}")


def run_body(args: dict) -> dict:
    """Map the Backtesting tab's arguments to a research request."""
    strategy = args.get("strategy") or {}
    body = {
        "strategy": strategy.get("type") or strategy.get("id") or "",
        "params": strategy.get("params") or {},
        "startDate": args.get("startDate"),
        "endDate": args.get("endDate"),
        "initialCapital": args.get("initialCapital", 100000),
        "commission": args.get("commission", 0.0005),
        "slippage": args.get("slippage", 0.0),
        "riskFreeRate": args.get("riskFreeRate", 0.0),
    }
    for key in ("paramRanges", "optimizeObjective", "optimizeMethod", "maxIterations", "wfSplits", "wfTrainRatio"):
        if key in args:
            body[key] = args[key]
    if not body["strategy"]:
        raise RuntimeError("choose an Autotrade strategy first")
    return body


def handle(command: str, args: dict):
    if command in ("test_connection", "initialize"):
        health = call("GET", "/health")
        return {"connected": True, "service": base_url(), **health}
    if command == "get_strategies":
        return {"provider": "autotrade", **call("GET", "/strategies")}
    if command == "get_command_options":
        return call("GET", "/command_options")
    if command == "run_backtest":
        return call("POST", "/backtest", run_body(args))
    if command == "optimize":
        return call("POST", "/optimize", run_body(args))
    if command == "walk_forward":
        return call("POST", "/walk_forward", run_body(args))
    raise RuntimeError(f"command '{command}' is not supported by the Autotrade provider "
                       "(use backtest, optimize or walk_forward)")


def main():
    if len(sys.argv) < 2:
        print(json.dumps({"success": False, "error": "usage: autotrade_provider.py <command> [json]"}))
        return
    command = sys.argv[1]
    try:
        raw = sys.stdin.read() if "--stdin" in sys.argv else (sys.argv[2] if len(sys.argv) > 2 else "{}")
        args = json.loads(raw or "{}")
        print(json.dumps({"success": True, "data": handle(command, args)}))
    except Exception as e:
        _log(f"{command} failed: {e}")
        print(json.dumps({"success": False, "error": str(e)}))


if __name__ == "__main__":
    main()
