# Action Record And Playback Plan

This document explains how to implement and test the first semantic action-record/playback example.

The goal is not to record raw mouse coordinates. The goal is to record app-level actions that can be replayed reliably:

```jsonl
{"type":"set","profile":"current","key":"latitude","value":20.0,"t":0.10}
{"type":"set","profile":"current","key":"world_axes_visible","value":true,"t":0.40}
{"type":"local_axis_rotation","profile":"current","axis":"z","degrees":15.0,"t":0.60}
```

## Implemented Example

The first implementation is a playback script:

- [tools/action_playback.py](tools/action_playback.py)
- [tools/action_record_example.jsonl](tools/action_record_example.jsonl)
- [tools/action_playback_tests.py](tools/action_playback_tests.py)

The script replays JSONL actions through the browser UI server API.

Detailed implementation notes and a dissection of the current `tools/action*.py` files are in [ACTION_RECORD_PLAYBACK_IMPLEMENTATION_NOTE.md](ACTION_RECORD_PLAYBACK_IMPLEMENTATION_NOTE.md).

Supported action types:

- `start_snapshot`: metadata marker; resets the playback cache
- `active_profile`: posts to `/api/active-profile`
- `set`: posts a profile field patch to `/api/state/current` or `/api/state/preset`
- `local_axis_rotation`: applies the same local-axis quaternion composition used by the UI Server
- `open_profile_file`: posts to `/api/profile-files/current/open`
- `image_key_changes`: posts to `/api/image-panel-key-changes`
- `wait`: sleeps for a fixed number of seconds

## How Playback Works

For a `set` action:

```json
{"type":"set","profile":"current","key":"latitude","value":20.0}
```

the script sends:

```text
POST /api/state/current
{"latitude":20.0}
```

For a local Z-axis rotation:

```json
{"type":"local_axis_rotation","profile":"current","axis":"z","degrees":15.0}
```

the script:

1. reads `/api/state`
2. reads the current profile quaternion
3. builds the delta quaternion for local `Z`
4. computes `current * delta`, matching the browser UI Server command row
5. posts the rounded quaternion components back to `/api/state/current`

## How To Run The Example

Start the UI server:

```bash
python3 tools/config_ui_server.py
```

Replay the example:

```bash
python3 tools/action_playback.py tools/action_record_example.jsonl --base-url http://127.0.0.1:8765
```

Replay with recorded timing:

```bash
python3 tools/action_playback.py tools/action_record_example.jsonl --base-url http://127.0.0.1:8765 --timed
```

Validate without posting:

```bash
python3 tools/action_playback.py tools/action_record_example.jsonl --dry-run
```

## How To Test

Run the unit tests:

```bash
python3 tools/action_playback_tests.py
```

The tests verify:

- `set` posts the expected profile patch
- `active_profile` posts the expected runtime state
- `local_axis_rotation` matches the browser UI quaternion multiplication order
- orientation locks reject local-axis playback
- JSONL parsing ignores blank lines and comment lines

## How To Add Automatic Recording

The next step is to add recorder hooks where actions already become semantic.

This is the current open issue: playback exists, but automatic recording hooks have not been added yet.

Browser UI Server hooks:

- `updateField(profileName, key, value)` records `set`
- `setActiveProfile(profileName)` records `active_profile`
- `openProfileFileAsCurrent(path)` records `open_profile_file`
- `applyOrientationCommand(profileName, axis, commandName, input)` records `local_axis_rotation`
- image panel actions record `set` or image file operations

Qt hooks:

- profile field setters record `set`
- visibility toggles record `set`
- globe-axis drag records either final quaternion `set` actions or a higher-level drag action with result quaternion
- profile/image commands record file operations

Recorder output should be JSONL, one action per line:

```text
assets/action_logs/session-YYYY-MM-DDTHH-MM-SS.jsonl
```

## Acceptance Criteria

The first accepted version should prove:

1. an action log can update Current and Preset through the UI server
2. the same log can be replayed twice without syntax errors
3. local-axis rotation playback gives the same quaternion as the browser UI command row
4. locked orientation fields block local-axis rotation playback
5. dry-run mode validates action logs before touching live profile files
