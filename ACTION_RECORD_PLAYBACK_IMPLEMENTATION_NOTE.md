# Action Record/Playback Implementation Note

This note documents the current automatic record/playback issue and dissects the current `tools/action*.py` implementation.

## Current Status

The current implementation is **semantic playback v0**.

Implemented:

- `tools/action_playback.py`: replays JSONL action logs through the browser UI server API
- `tools/action_playback_tests.py`: unit tests for the playback core
- `tools/action_record_example.jsonl`: example semantic action log

Not implemented yet:

- automatic action recording from the browser UI Server
- automatic action recording from the Qt app
- a recorder start/stop UI
- persistent action-log file management under `assets/action_logs/`
- file-operation replay for every profile/image browser operation

The main issue is therefore:

```text
Playback exists, but automatic recording hooks do not exist yet.
```

The current playback script proves the action format and replay path first. The recorder should be added after the action schema is accepted.

## Why Semantic Actions

The project should record app-level actions, not raw GUI events.

Avoid this:

```json
{"type":"mouse_move","x":942,"y":531}
{"type":"mouse_click","button":"left"}
```

Prefer this:

```json
{"type":"set","profile":"current","key":"latitude","value":20.0}
{"type":"set","profile":"current","key":"world_axes_visible","value":true}
{"type":"local_axis_rotation","profile":"current","axis":"z","degrees":15.0}
```

Raw GUI events depend on window size, monitor position, focus, menu state, font scale, and timing. Semantic actions target the project state model and are much more stable.

## Files Matching `tools/action*.py`

Current matching Python files:

- [tools/action_playback.py](tools/action_playback.py)
- [tools/action_playback_tests.py](tools/action_playback_tests.py)

Related non-Python example file:

- [tools/action_record_example.jsonl](tools/action_record_example.jsonl)

There is intentionally no `tools/action_record.py` yet. The automatic recorder is the next implementation step.

## `tools/action_playback.py` Dissection

### Purpose

`tools/action_playback.py` replays semantic action logs into a running browser UI server.

Default server target:

```text
http://127.0.0.1:8765
```

The script talks to existing UI server endpoints:

- `GET /api/state`
- `POST /api/state/current`
- `POST /api/state/preset`
- `POST /api/active-profile`
- `POST /api/profile-files/current/open`
- `POST /api/image-panel-key-changes`

### Constants

```python
PROFILE_NAMES = {"current", "preset"}
```

Only the two profile columns are accepted.

```python
ORIENTATION_KEYS = {
    "w": "globe_orientation_scalar",
    "x": "globe_orientation_x",
    "y": "globe_orientation_y",
    "z": "globe_orientation_z",
}
```

This maps short quaternion component names to profile JSON keys.

```python
ORIENTATION_LOCK_KEYS = [
    "globe_orientation_scalar_locked",
    "globe_orientation_x_locked",
    "globe_orientation_y_locked",
    "globe_orientation_z_locked",
]
```

Playback refuses `local_axis_rotation` if any quaternion component is locked. This matches the UI Server command rows, which disable local-axis rotations while the orientation is locked.

### `ActionError`

```python
class ActionError(RuntimeError):
```

`ActionError` is the script-level failure type. It is used for invalid logs, unsupported action types, bad profile names, HTTP failures, and locked-orientation playback failures.

The CLI catches `ActionError`, prints one error line, and returns exit code `1`.

### `HttpJsonClient`

`HttpJsonClient` is a small HTTP adapter.

Responsibilities:

- hold the base URL
- issue JSON `GET`
- issue JSON `POST`
- convert UI server HTTP errors into `ActionError`

Important behavior:

```python
self.base_url = base_url.rstrip("/")
```

This lets both `http://127.0.0.1:8765` and `http://127.0.0.1:8765/` work.

```python
urllib.request.urlopen(..., timeout=10)
```

The script uses only Python standard library networking. No `requests` dependency is needed.

### `ActionPlayer`

`ActionPlayer` is the playback engine.

Constructor fields:

- `client`: any object with `get_json(path)` and `post_json(path, payload)`
- `dry_run`: print actions without changing the server
- `timed`: respect action timestamp field `t`
- `state`: cached `/api/state` response
- `previous_timestamp`: previous action timestamp for timed replay

The tests use a fake in-memory client. That is why `client` is typed loosely as `Any`.

### `apply_actions(...)`

```python
def apply_actions(self, actions: Iterable[dict[str, Any]]) -> int:
```

Loops through action objects, applies each one, and returns the count. This is the top-level API used by the CLI and tests.

### `apply_action(...)`

`apply_action` is the dispatcher.

It validates:

- each action must be a JSON object
- each action must have a non-empty `type`

Then it routes by `type`:

| Action type | Handler |
| --- | --- |
| `start_snapshot` | reset cached state |
| `active_profile` | `set_active_profile(...)` |
| `set_active_profile` | alias for `active_profile` |
| `set` | `set_field(...)` |
| `local_axis_rotation` | `apply_local_axis_rotation(...)` |
| `open_profile_file` | `open_profile_file(...)` |
| `image_key_changes` | `update_image_key_changes(...)` |
| `wait` | sleep fixed seconds |

Unknown action types fail immediately.

Dry-run behavior:

```python
if self.dry_run:
    print(json.dumps(action, sort_keys=True))
    return
```

Dry-run validates JSONL parsing and action object shape, then prints actions. It does not validate every server-dependent rule, because it intentionally does not call `/api/state`.

### `start_snapshot`

Current implementation:

```python
if action_type == "start_snapshot":
    self.state = None
    return
```

The action is metadata today. It tells playback to discard cached server state. It does not yet restore snapshot files.

Future behavior could add:

- verify snapshot paths exist
- restore `current_state.json`
- restore `preset_state.json`
- restore `ui_runtime_state.json`

### `set_active_profile(...)`

Input example:

```json
{"type":"active_profile","profile":"preset"}
```

HTTP request:

```text
POST /api/active-profile
{"activeProfile":"preset"}
```

Then the cached state is updated:

```python
self.state["activeProfile"] = payload.get("activeProfile", profile_name)
```

This is enough for browser/Qt synchronization because the UI server writes `assets/ui_runtime_state.json`.

### `set_field(...)`

Input example:

```json
{"type":"set","profile":"current","key":"latitude","value":20.0}
```

HTTP request:

```text
POST /api/state/current
{"latitude":20.0}
```

The UI server validates/coerces the field through its normal schema. Unknown fields are ignored by the server. Playback intentionally lets the server be the authority for field validity.

### `apply_local_axis_rotation(...)`

Input example:

```json
{"type":"local_axis_rotation","profile":"current","axis":"z","degrees":15.0}
```

This action mirrors the UI Server command rows:

- `Roll (Z) deg`
- `Yaw (Y) deg`
- `Pitch (X) deg`

Validation:

- profile must be `current` or `preset`
- axis must be `x`, `y`, or `z`
- degrees must be finite
- zero degrees are ignored
- orientation locks reject playback

Computation:

```text
current = normalized current profile quaternion
delta   = axis quaternion(axis, degrees)
result  = current * delta
```

The multiplication order is important. `current * delta` means the delta is applied in the globe-local/body axis frame. This matches the browser UI implementation.

The posted patch is rounded to two decimals:

```python
patch = {
    "globe_orientation_scalar": js_round(result["w"], 2),
    "globe_orientation_x": js_round(result["x"], 2),
    "globe_orientation_y": js_round(result["y"], 2),
    "globe_orientation_z": js_round(result["z"], 2),
}
```

That is intentionally aligned with the current UI Server numerical format.

### `open_profile_file(...)`

Input example:

```json
{"type":"open_profile_file","path":"/abs/path/to/profile.json"}
```

HTTP request:

```text
POST /api/profile-files/current/open
{"path":"/abs/path/to/profile.json"}
```

This replaces live Current with the selected profile file through the same UI server path as the browser `Open` button.

After opening, playback clears its cached state:

```python
self.state = None
```

That forces the next state-dependent action to re-read `/api/state`.

### `update_image_key_changes(...)`

Input example:

```json
{"type":"image_key_changes","text":"New note"}
```

HTTP request:

```text
POST /api/image-panel-key-changes
{"text":"New note"}
```

This updates the persisted Image panel note in `assets/ui_runtime_state.json`.

### `post_profile_patch(...)`

Shared helper for profile updates.

It posts a patch to:

```text
/api/state/{profile_name}
```

Then it expects the UI server response to include:

```json
{"profile": {...}}
```

It updates the cached profile so later actions, such as local-axis rotations, use the new state.

### `profile(...)` And `ensure_state(...)`

`ensure_state()` lazily fetches:

```text
GET /api/state
```

`profile(profile_name)` returns a deep copy of the cached profile.

Deep copy matters because action handlers should not mutate cached profile dictionaries directly unless the server accepted a posted change.

### `_apply_timing(...)`

Timed playback uses each action's `t` field:

```json
{"type":"set","key":"zoom","value":0.81,"t":0.30}
```

If `--timed` is enabled, playback sleeps for the difference between consecutive timestamps:

```text
sleep(max(0, current_t - previous_t))
```

If `--timed` is not enabled, timestamps are ignored.

### `read_jsonl(...)`

Reads one JSON action per line.

Rules:

- blank lines are ignored
- lines starting with `#` are ignored
- every non-comment line must parse as a JSON object
- invalid JSON reports file path and line number

This makes action logs easy to inspect and edit manually.

### Quaternion Helpers

`normalized_quaternion_from_profile(...)` reads `globe_orientation_*` fields from a profile and normalizes them.

`axis_quaternion(axis, degrees)` builds a normalized delta quaternion for one local axis.

`multiply_quaternions(a, b)` implements:

```text
a * b
```

using the standard Hamilton product.

`normalize_quaternion(q)` guards against invalid or near-zero quaternions and falls back to identity.

### `js_round(...)`

The function currently implements positive-half-up rounding:

```python
math.floor(value * scale + 0.5) / scale
```

This matches the current expected positive quaternion test cases. If future playback must exactly match JavaScript `Math.round` for negative half values, add tests before changing this function.

### CLI Entry Points

Arguments:

- `log`: required JSONL file path
- `--base-url`: UI server base URL, default `http://127.0.0.1:8765`
- `--timed`: respect `t`
- `--dry-run`: print actions without posting

Main flow:

```text
parse args
read JSONL
create HttpJsonClient
create ActionPlayer
apply actions
print applied count
```

## `tools/action_playback_tests.py` Dissection

### Purpose

The tests verify playback logic without starting the UI server.

They use a fake client:

```python
class MemoryClient:
```

This makes tests fast, deterministic, and independent of files in `assets/`.

### `default_profile()`

Returns the minimal profile state needed by tests:

- latitude
- longitude
- world axes visibility
- identity globe quaternion
- orientation lock fields

The test profile is not a full app profile. It only includes fields used by the test cases.

### `MemoryClient`

`MemoryClient` implements the same interface used by `ActionPlayer`:

- `get_json(path)`
- `post_json(path, payload)`

Supported fake endpoints:

- `/api/state`
- `/api/state/current`
- `/api/state/preset`
- `/api/active-profile`
- `/api/profile-files/current/open`
- `/api/image-panel-key-changes`

It records every post:

```python
self.posts: list[tuple[str, dict]] = []
```

That lets tests assert the exact endpoint and payload.

### Test Cases

`test_set_action_posts_profile_patch`

- applies a `set` action
- verifies the fake Current profile changed
- verifies the final post was `/api/state/current`

`test_active_profile_action_posts_runtime_state`

- applies an `active_profile` action
- verifies fake runtime state changed to `preset`
- verifies the endpoint was `/api/active-profile`

`test_local_axis_rotation_matches_browser_order`

- applies 90 degrees around local `z`
- expects quaternion approximately:

```text
w = 0.71
x = 0.00
y = 0.00
z = 0.71
```

This proves the script uses the same composition direction as the browser UI Server local-axis command.

`test_local_axis_rotation_rejects_locked_orientation`

- locks one orientation component
- verifies local-axis rotation raises `ActionError`

This protects the lock semantics.

`test_read_jsonl_skips_blank_lines_and_comments`

- writes a temporary JSONL file
- includes a blank line and a comment
- verifies only the action line is returned

## `tools/action_record_example.jsonl` Dissection

This file is the sample action log.

Line 1:

```json
{"type":"start_snapshot","current":"assets/current_state.json","preset":"assets/preset_state.json","active":"current","t":0.0}
```

Documents the intended starting files and active mode. In v0 this is metadata and cache reset only.

Line 2:

```json
{"type":"active_profile","profile":"current","t":0.05}
```

Switches the synchronized active mode to Current.

Lines 3-5:

```json
{"type":"set","profile":"current","key":"latitude","value":20.0,"t":0.10}
{"type":"set","profile":"current","key":"longitude","value":35.0,"t":0.20}
{"type":"set","profile":"current","key":"zoom","value":0.81,"t":0.30}
```

Apply basic view settings through `/api/state/current`.

Line 6:

```json
{"type":"set","profile":"current","key":"world_axes_visible","value":true,"t":0.40}
```

Turns on the globe/world axes visibility flag.

Line 7:

```json
{"type":"set","profile":"current","key":"fisheye_image_path","value":"GOPR0308.JPG","t":0.50}
```

Sets the active fisheye image path. The UI server normalizes HFC-relative image paths through its normal profile update path.

Line 8:

```json
{"type":"local_axis_rotation","profile":"current","axis":"z","degrees":15.0,"t":0.60}
```

Applies one local Z-axis rotation by updating the stored quaternion.

## Automatic Recorder Design

The automatic recorder should write the same JSONL shape that `action_playback.py` already consumes.

### Browser UI Server Hooks

Recommended hooks:

| UI Server function | Record action |
| --- | --- |
| `updateField(profileName, key, value)` | `set` |
| `setActiveProfile(profileName)` | `active_profile` |
| `openProfileFileAsCurrent(path)` | `open_profile_file` |
| `applyOrientationCommand(profileName, axis, commandName, input)` | `local_axis_rotation` |
| `updateImageKeyChanges(...)` | `image_key_changes` |
| image `Open` | `set` for `fisheye_image_path` |
| image `Save`/`Rename`/`Delete` | future file-operation actions |
| profile `Save`/`Rename`/`Delete` | future file-operation actions |

The browser recorder should not record field updates while replay is applying a log unless the user explicitly wants nested recording.

### Qt App Hooks

Recommended Qt hooks:

- state field setters record `set`
- right-click visibility toggles record `set`
- active profile switch records `active_profile`
- local-axis command equivalent records `local_axis_rotation`
- globe-axis drag records final quaternion fields or a future `globe_axis_drag` semantic action
- profile/image file operations record future file-operation actions

### Recorder State

Add a small recorder state:

```text
recording: on/off
start_time_monotonic
output_path
suppress_recording_while_replaying
```

Each recorded action gets:

```json
{"type":"set","profile":"current","key":"latitude","value":20.0,"t":1.234}
```

where `t` is seconds since recording started.

### Output Path

Recommended action-log directory:

```text
assets/action_logs/
```

Recommended filename:

```text
session-YYYY-MM-DDTHH-MM-SS.jsonl
```

## Current Risks And Gaps

- `start_snapshot` does not restore snapshots yet.
- `--dry-run` does not validate server-dependent fields or locks.
- There is no recorder hook layer yet.
- File operations are only partially covered by playback.
- The playback script depends on the UI server being reachable.
- The current local-axis rotation path rounds quaternion components to two decimals, matching UI display but losing precision.
- Playback mutates live `assets/current_state.json`, `assets/preset_state.json`, and `assets/ui_runtime_state.json`.

## Test Plan For The Next Recorder Step

Add unit tests for:

- recorder timestamp generation
- recorder JSONL append format
- recording suppression during playback
- browser `updateField(...)` emitting a `set` action
- active profile switch emitting `active_profile`
- local-axis command emitting `local_axis_rotation`

Add integration tests when practical:

1. start a temporary UI server against a temporary assets directory
2. apply UI actions
3. verify the generated JSONL
4. replay the JSONL into a fresh temporary assets directory
5. compare final Current/Preset/runtime state

