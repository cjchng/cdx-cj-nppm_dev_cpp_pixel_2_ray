#!/usr/bin/env python3
"""Replay Pixel To Ray semantic action logs through the browser UI API."""

from __future__ import annotations

import argparse
import copy
import json
import math
import sys
import time
import urllib.error
import urllib.request
from pathlib import Path
from typing import Any, Iterable


PROFILE_NAMES = {"current", "preset"}
ORIENTATION_KEYS = {
    "w": "globe_orientation_scalar",
    "x": "globe_orientation_x",
    "y": "globe_orientation_y",
    "z": "globe_orientation_z",
}
ORIENTATION_LOCK_KEYS = [
    "globe_orientation_scalar_locked",
    "globe_orientation_x_locked",
    "globe_orientation_y_locked",
    "globe_orientation_z_locked",
]


class ActionError(RuntimeError):
    """Raised when an action log entry cannot be applied."""


class HttpJsonClient:
    def __init__(self, base_url: str) -> None:
        self.base_url = base_url.rstrip("/")

    def get_json(self, path: str) -> dict[str, Any]:
        with urllib.request.urlopen(self._url(path), timeout=10) as response:
            return json.loads(response.read().decode("utf-8"))

    def post_json(self, path: str, payload: dict[str, Any]) -> dict[str, Any]:
        body = json.dumps(payload).encode("utf-8")
        request = urllib.request.Request(
            self._url(path),
            data=body,
            headers={"Content-Type": "application/json"},
            method="POST",
        )
        try:
            with urllib.request.urlopen(request, timeout=10) as response:
                return json.loads(response.read().decode("utf-8") or "{}")
        except urllib.error.HTTPError as error:
            detail = error.read().decode("utf-8", errors="replace")
            raise ActionError(f"POST {path} failed: {error.code} {detail}") from error

    def _url(self, path: str) -> str:
        suffix = path if path.startswith("/") else f"/{path}"
        return f"{self.base_url}{suffix}"


class ActionPlayer:
    def __init__(self, client: Any, *, dry_run: bool = False, timed: bool = False) -> None:
        self.client = client
        self.dry_run = dry_run
        self.timed = timed
        self.state: dict[str, Any] | None = None
        self.previous_timestamp: float | None = None

    def apply_actions(self, actions: Iterable[dict[str, Any]]) -> int:
        count = 0
        for action in actions:
            self.apply_action(action)
            count += 1
        return count

    def apply_action(self, action: dict[str, Any]) -> None:
        if not isinstance(action, dict):
            raise ActionError("Action must be a JSON object.")
        self._apply_timing(action)
        action_type = str(action.get("type", "")).strip()
        if not action_type:
            raise ActionError("Action is missing type.")

        if self.dry_run:
            print(json.dumps(action, sort_keys=True))
            return

        if action_type == "start_snapshot":
            self.state = None
            return
        if action_type in {"active_profile", "set_active_profile"}:
            self.set_active_profile(action)
            return
        if action_type == "set":
            self.set_field(action)
            return
        if action_type == "local_axis_rotation":
            self.apply_local_axis_rotation(action)
            return
        if action_type == "open_profile_file":
            self.open_profile_file(action)
            return
        if action_type == "image_key_changes":
            self.update_image_key_changes(action)
            return
        if action_type == "wait":
            time.sleep(float(action.get("seconds", 0)))
            return

        raise ActionError(f"Unsupported action type: {action_type}")

    def set_active_profile(self, action: dict[str, Any]) -> None:
        profile_name = normalize_profile_name(action.get("profile", action.get("activeProfile")))
        payload = self.client.post_json("/api/active-profile", {"activeProfile": profile_name})
        self.ensure_state()
        self.state["activeProfile"] = payload.get("activeProfile", profile_name)

    def set_field(self, action: dict[str, Any]) -> None:
        profile_name = normalize_profile_name(action.get("profile", "current"))
        key = str(action.get("key", "")).strip()
        if not key:
            raise ActionError("set action requires key.")
        if "value" not in action:
            raise ActionError("set action requires value.")
        self.post_profile_patch(profile_name, {key: action["value"]})

    def apply_local_axis_rotation(self, action: dict[str, Any]) -> None:
        profile_name = normalize_profile_name(action.get("profile", "current"))
        axis = str(action.get("axis", "")).strip().lower()
        if axis not in {"x", "y", "z"}:
            raise ActionError("local_axis_rotation axis must be x, y, or z.")
        degrees = float(action.get("degrees", 0))
        if not math.isfinite(degrees):
            raise ActionError("local_axis_rotation degrees must be finite.")
        if abs(degrees) <= 1e-8:
            return

        profile = self.profile(profile_name)
        if any(bool(profile.get(key)) for key in ORIENTATION_LOCK_KEYS):
            raise ActionError(f"{profile_name} globe orientation is locked.")

        current = normalized_quaternion_from_profile(profile)
        delta = axis_quaternion(axis, degrees)
        result = multiply_quaternions(current, delta)
        patch = {
            ORIENTATION_KEYS["w"]: js_round(result["w"], 2),
            ORIENTATION_KEYS["x"]: js_round(result["x"], 2),
            ORIENTATION_KEYS["y"]: js_round(result["y"], 2),
            ORIENTATION_KEYS["z"]: js_round(result["z"], 2),
        }
        self.post_profile_patch(profile_name, patch)

    def open_profile_file(self, action: dict[str, Any]) -> None:
        path = str(action.get("path", "")).strip()
        if not path:
            raise ActionError("open_profile_file requires path.")
        self.client.post_json("/api/profile-files/current/open", {"path": path})
        self.state = None

    def update_image_key_changes(self, action: dict[str, Any]) -> None:
        self.client.post_json("/api/image-panel-key-changes", {"text": str(action.get("text", ""))})
        self.state = None

    def post_profile_patch(self, profile_name: str, patch: dict[str, Any]) -> dict[str, Any]:
        payload = self.client.post_json(f"/api/state/{profile_name}", patch)
        profile = payload.get("profile")
        if not isinstance(profile, dict):
            raise ActionError(f"POST /api/state/{profile_name} did not return a profile.")
        self.ensure_state()
        self.state.setdefault("profiles", {})[profile_name] = profile
        return profile

    def profile(self, profile_name: str) -> dict[str, Any]:
        self.ensure_state()
        profiles = self.state.get("profiles", {})
        profile = profiles.get(profile_name)
        if not isinstance(profile, dict):
            raise ActionError(f"Server state does not include {profile_name} profile.")
        return copy.deepcopy(profile)

    def ensure_state(self) -> dict[str, Any]:
        if self.state is None:
            self.state = self.client.get_json("/api/state")
        return self.state

    def _apply_timing(self, action: dict[str, Any]) -> None:
        if not self.timed or "t" not in action:
            return
        timestamp = float(action["t"])
        if self.previous_timestamp is not None:
            time.sleep(max(0.0, timestamp - self.previous_timestamp))
        self.previous_timestamp = timestamp


def read_jsonl(path: Path) -> list[dict[str, Any]]:
    actions: list[dict[str, Any]] = []
    with path.open("r", encoding="utf-8") as handle:
        for line_number, raw_line in enumerate(handle, 1):
            line = raw_line.strip()
            if not line or line.startswith("#"):
                continue
            try:
                value = json.loads(line)
            except json.JSONDecodeError as error:
                raise ActionError(f"{path}:{line_number}: invalid JSONL: {error}") from error
            if not isinstance(value, dict):
                raise ActionError(f"{path}:{line_number}: action must be a JSON object.")
            actions.append(value)
    return actions


def normalize_profile_name(value: Any) -> str:
    profile_name = str(value or "current").strip().lower()
    if profile_name not in PROFILE_NAMES:
        raise ActionError(f"Unknown profile: {profile_name}")
    return profile_name


def normalized_quaternion_from_profile(profile: dict[str, Any]) -> dict[str, float]:
    q = {
        "w": float(profile.get(ORIENTATION_KEYS["w"], 1.0)),
        "x": float(profile.get(ORIENTATION_KEYS["x"], 0.0)),
        "y": float(profile.get(ORIENTATION_KEYS["y"], 0.0)),
        "z": float(profile.get(ORIENTATION_KEYS["z"], 0.0)),
    }
    return normalize_quaternion(q)


def axis_quaternion(axis: str, degrees: float) -> dict[str, float]:
    half_radians = math.radians(degrees) * 0.5
    vector = math.sin(half_radians)
    return normalize_quaternion({
        "w": math.cos(half_radians),
        "x": vector if axis == "x" else 0.0,
        "y": vector if axis == "y" else 0.0,
        "z": vector if axis == "z" else 0.0,
    })


def multiply_quaternions(a: dict[str, float], b: dict[str, float]) -> dict[str, float]:
    return normalize_quaternion({
        "w": a["w"] * b["w"] - a["x"] * b["x"] - a["y"] * b["y"] - a["z"] * b["z"],
        "x": a["w"] * b["x"] + a["x"] * b["w"] + a["y"] * b["z"] - a["z"] * b["y"],
        "y": a["w"] * b["y"] - a["x"] * b["z"] + a["y"] * b["w"] + a["z"] * b["x"],
        "z": a["w"] * b["z"] + a["x"] * b["y"] - a["y"] * b["x"] + a["z"] * b["w"],
    })


def normalize_quaternion(q: dict[str, float]) -> dict[str, float]:
    length = math.sqrt(q["w"] ** 2 + q["x"] ** 2 + q["y"] ** 2 + q["z"] ** 2)
    if not math.isfinite(length) or length <= 1e-8:
        return {"w": 1.0, "x": 0.0, "y": 0.0, "z": 0.0}
    return {key: value / length for key, value in q.items()}


def js_round(value: float, decimals: int) -> float:
    scale = 10 ** decimals
    return math.floor(value * scale + 0.5) / scale


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log", type=Path, help="JSONL action log to replay.")
    parser.add_argument("--base-url", default="http://127.0.0.1:8765", help="UI server base URL.")
    parser.add_argument("--timed", action="store_true", help="Respect action timestamp field `t`.")
    parser.add_argument("--dry-run", action="store_true", help="Validate and print actions without posting them.")
    return parser.parse_args(argv)


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    try:
        actions = read_jsonl(args.log)
        player = ActionPlayer(HttpJsonClient(args.base_url), dry_run=args.dry_run, timed=args.timed)
        count = player.apply_actions(actions)
    except ActionError as error:
        print(f"action playback failed: {error}", file=sys.stderr)
        return 1
    print(f"applied {count} actions")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
