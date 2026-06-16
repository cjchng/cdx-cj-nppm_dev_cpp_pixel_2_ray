#!/usr/bin/env python3
"""Unit tests for tools/action_playback.py."""

from __future__ import annotations

import copy
import tempfile
import unittest
from pathlib import Path

from action_playback import ActionError, ActionPlayer, read_jsonl


def default_profile() -> dict:
    return {
        "latitude": 0.0,
        "longitude": 0.0,
        "world_axes_visible": False,
        "globe_orientation_scalar": 1.0,
        "globe_orientation_x": 0.0,
        "globe_orientation_y": 0.0,
        "globe_orientation_z": 0.0,
        "globe_orientation_scalar_locked": False,
        "globe_orientation_x_locked": False,
        "globe_orientation_y_locked": False,
        "globe_orientation_z_locked": False,
    }


class MemoryClient:
    def __init__(self) -> None:
        self.state = {
            "profiles": {
                "current": default_profile(),
                "preset": default_profile(),
            },
            "activeProfile": "current",
            "currentProfileName": "Current",
        }
        self.posts: list[tuple[str, dict]] = []

    def get_json(self, path: str) -> dict:
        if path != "/api/state":
            raise AssertionError(path)
        return copy.deepcopy(self.state)

    def post_json(self, path: str, payload: dict) -> dict:
        self.posts.append((path, copy.deepcopy(payload)))
        if path in {"/api/state/current", "/api/state/preset"}:
            profile_name = path.rsplit("/", 1)[-1]
            self.state["profiles"][profile_name].update(payload)
            return {
                "profile": copy.deepcopy(self.state["profiles"][profile_name]),
                "activeProfile": self.state["activeProfile"],
                "currentProfileName": self.state["currentProfileName"],
            }
        if path == "/api/active-profile":
            self.state["activeProfile"] = payload["activeProfile"]
            return {"activeProfile": self.state["activeProfile"]}
        if path == "/api/profile-files/current/open":
            self.state["profiles"]["current"]["opened_path"] = payload["path"]
            return {"profile": copy.deepcopy(self.state["profiles"]["current"])}
        if path == "/api/image-panel-key-changes":
            self.state["imagePanelKeyChanges"] = payload["text"]
            return {"imagePanelKeyChanges": payload["text"]}
        raise AssertionError(path)


class ActionPlaybackTests(unittest.TestCase):
    def test_set_action_posts_profile_patch(self) -> None:
        client = MemoryClient()
        player = ActionPlayer(client)

        player.apply_action({
            "type": "set",
            "profile": "current",
            "key": "latitude",
            "value": 20.25,
        })

        self.assertEqual(client.state["profiles"]["current"]["latitude"], 20.25)
        self.assertEqual(client.posts[-1], ("/api/state/current", {"latitude": 20.25}))

    def test_active_profile_action_posts_runtime_state(self) -> None:
        client = MemoryClient()
        player = ActionPlayer(client)

        player.apply_action({"type": "active_profile", "profile": "preset"})

        self.assertEqual(client.state["activeProfile"], "preset")
        self.assertEqual(client.posts[-1], ("/api/active-profile", {"activeProfile": "preset"}))

    def test_local_axis_rotation_matches_browser_order(self) -> None:
        client = MemoryClient()
        player = ActionPlayer(client)

        player.apply_action({
            "type": "local_axis_rotation",
            "profile": "current",
            "axis": "z",
            "degrees": 90,
        })

        current = client.state["profiles"]["current"]
        self.assertEqual(current["globe_orientation_scalar"], 0.71)
        self.assertEqual(current["globe_orientation_x"], 0.0)
        self.assertEqual(current["globe_orientation_y"], 0.0)
        self.assertEqual(current["globe_orientation_z"], 0.71)

    def test_local_axis_rotation_rejects_locked_orientation(self) -> None:
        client = MemoryClient()
        client.state["profiles"]["current"]["globe_orientation_z_locked"] = True
        player = ActionPlayer(client)

        with self.assertRaises(ActionError):
            player.apply_action({
                "type": "local_axis_rotation",
                "profile": "current",
                "axis": "z",
                "degrees": 10,
            })

    def test_read_jsonl_skips_blank_lines_and_comments(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "actions.jsonl"
            path.write_text(
                "\n"
                "# comment\n"
                "{\"type\":\"set\",\"profile\":\"current\",\"key\":\"zoom\",\"value\":0.8}\n",
                encoding="utf-8",
            )

            actions = read_jsonl(path)

        self.assertEqual(len(actions), 1)
        self.assertEqual(actions[0]["key"], "zoom")


if __name__ == "__main__":
    unittest.main()
