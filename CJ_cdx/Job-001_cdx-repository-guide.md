# Pixel To Ray Repository Guide

## 1. Purpose and current scope

This repository implements an interactive fisheye-image-to-3D-globe visualization. The primary application is a C++17/Qt6 desktop program with a custom OpenGL viewport. A companion Python HTTP server exposes the same JSON profile state through a browser UI, and a second Python tool replays semantic JSONL actions through that server.

The codebase currently supports:

- a textured spherical cap generated from a circular fisheye image;
- a globe-local coordinate frame anchored by latitude, longitude, and roll;
- a separate quaternion that orients the entire globe in world space;
- global axis traces, a y-axis great-circle trace, and an x-tangential cylinder;
- orthographic and perspective camera projection;
- mouse and keyboard manipulation of the anchor, component lengths, zoom, and globe orientation;
- Current and Preset JSON profiles, per-field locks, and runtime UI state;
- a native Qt control overlay and a browser configuration UI;
- saved-profile and HFC-image file management in the browser UI; and
- semantic action playback for repeatable demonstrations.

The checked-out branch at the time of this guide is `Auto-Demo-Script-Development` at commit `c27c4fb` (`Will Automatic Scripting Demo`). Some older notes still name `master`; treat the Git state, source code, and this guide as more current than those historical statements.

## 2. System architecture

```text
                           assets/*.json
                         shared profile state
                       + runtime UI metadata
                                  |
                QFileSystemWatcher|       atomic JSON reads/writes
                                  |
       +--------------------------+--------------------------+
       |                                                     |
+------v------------------+                    +-------------v-------------+
| Qt6 desktop application |                    | Python browser UI server  |
|                         |                    |                           |
| MainWindow              |                    | ConfigStore               |
| - profile ownership     |                    | - schema/coercion         |
| - native controls       |                    | - profile/image manager   |
| - file watcher          |                    |                           |
|          |              |                    | ThreadingHTTPServer        |
| PixelToRayWidget        |                    | - embedded HTML/CSS/JS    |
| - geometry/state        |                    | - JSON API                |
| - OpenGL rendering      |                    +-------------^-------------+
| - picking/input         |                                  |
+-------------------------+                                  | HTTP JSON
                                                             |
                                               +-------------+-------------+
                                               | Semantic action playback  |
                                               | action_playback.py        |
                                               +---------------------------+
```

There is no direct socket connection between the Qt application and the browser. Their integration boundary is the JSON files in `assets/`. The playback client is different: it talks to the browser server's HTTP API, and the server then updates those same files.

## 3. Repository layout

| Path | Role |
|---|---|
| [`CMakeLists.txt`](../CMakeLists.txt) | Defines the C++17 build, Qt6 dependencies, application, shared core library, and two CTest targets. |
| [`src/main.cpp`](../src/main.cpp) | Configures the OpenGL surface, runs math sanity checks, constructs `QApplication`, and enters the event loop. |
| [`src/MainWindow.*`](../src/MainWindow.h) | Owns profiles, JSON persistence, file watching, the menu bar, and the native Control Table overlay. |
| [`src/PixelToRayWidget.*`](../src/PixelToRayWidget.h) | Owns runtime scene state, OpenGL resources, geometry generation, fisheye sampling, picking, and viewport input. |
| [`src/FrameMath.*`](../src/FrameMath.h) | Testable coordinate-frame and component helpers used by the viewport. |
| [`src/InteractionLogic.*`](../src/InteractionLogic.h) | Testable 2D handle picking and screen-drag projection helpers. |
| [`tests/`](../tests) | QtTest coverage for `FrameMath` and `InteractionLogic`. |
| [`tools/config_ui_server.py`](../tools/config_ui_server.py) | Standard-library HTTP server with an embedded single-page browser UI and JSON/file-management API. |
| [`tools/action_playback.py`](../tools/action_playback.py) | Standard-library semantic JSONL playback client. |
| [`tools/action_playback_tests.py`](../tools/action_playback_tests.py) | Five in-memory unit tests for playback behavior. |
| [`tools/action_record_example.jsonl`](../tools/action_record_example.jsonl) | Eight-action demonstration log. |
| `assets/` | Live profiles, runtime metadata, fisheye images, test images, screenshots, and saved profiles. In this checkout it is untracked. |
| Root `*.md` files | User instructions, workflow notes, implementation records, and future design plans. |
| `build/` | Default CMake build tree; ignored by Git. |
| `cmake-build-debug/` | IDE-generated build tree; currently present but not covered by `.gitignore`. |
| `src/*.2bd` and `src/*.before-*` | Historical source snapshots, not build inputs. |

Only `build/` is listed in [`.gitignore`](../.gitignore). At the time of inspection, `.codex/`, `.idea/`, `.venv/`, `assets/`, `packages.microsoft.gpg`, and the empty `requirements.txt` are untracked. Do not assume a fresh clone contains the runtime images or profile files visible in this workspace.

## 4. Build system and dependencies

The project requires CMake 3.16 or newer, a C++17 compiler, Qt6 Widgets/Gui/Test/OpenGL/OpenGLWidgets, and system OpenGL development files. The Ubuntu package list and historical setup failures are documented in [`004-SETUP_AND_TROUBLESHOOTING.md`](../004-SETUP_AND_TROUBLESHOOTING.md).

The CMake graph is intentionally small:

| Target | Sources | Important links |
|---|---|---|
| `pixel_2_ray_core` | `FrameMath.cpp`, `InteractionLogic.cpp` | `Qt6::Gui` |
| `pixel_2_ray_qt` | `main.cpp`, `MainWindow.cpp`, `PixelToRayWidget.cpp` | core, Widgets, Gui, OpenGL, OpenGLWidgets |
| `pixel_2_ray_frame_tests` | `FrameMathTests.cpp` | core, Test, Gui |
| `pixel_2_ray_interaction_tests` | `InteractionLogicTests.cpp` | core, Test, Gui |

`CMAKE_AUTOMOC`, `CMAKE_AUTOUIC`, and `CMAKE_AUTORCC` are enabled. The project has no `.ui` or `.qrc` files today; widgets and shaders are authored directly in C++.

Normal build and test commands:

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
./build/pixel_2_ray_qt
```

The application requests an OpenGL 3.2 core profile, a 24-bit depth buffer, an 8-bit stencil buffer, and no multisampling in [`main.cpp`](../src/main.cpp#L10). Shaders use GLSL 1.50 core syntax for compatibility with that context.

During documentation validation on 2026-08-16, configuration and compilation succeeded. CMake emitted non-fatal warnings for missing XKB and Vulkan headers; neither prevented this target set from building.

## 5. Desktop application lifecycle

### Startup

1. `main()` calls `FrameMath::runSanityChecks()` before creating the GUI.
2. It installs the default OpenGL surface format.
3. `MainWindow` resolves the repository `assets/` directory relative to the current directory or executable.
4. It constructs built-in defaults and then layers legacy and current profile files over them.
5. It writes normalized Preset and Current profiles back to disk.
6. It builds the full-window `PixelToRayWidget` and an initially hidden Control Table overlay.
7. It connects controls, profile switching, file watching, menu commands, and UI font shortcuts.
8. The event loop begins; OpenGL resources are created lazily when the widget initializes or draws.

The startup profile fallback chain is deliberately backward compatible. Preset state can come from legacy app-data `default_state.json`, workspace `assets/default_state.json`, and finally `assets/preset_state.json`; Current has a similar legacy fallback. New writes use `preset_state.json` and `current_state.json`.

### `MainWindow`: UI and persistence owner

`MainWindow::PanelState` mirrors every persisted profile field. It contains view parameters, globe orientation, camera pose, visibility flags, five editable component lengths, the image path, a profile comment, and lock flags.

The native overlay is organized into six menu-accessible sections:

1. Status
2. View
3. Globe Coordinates
4. Global Orientation
5. Camera Pose
6. Components

Each row compares editable Current state with read-only Preset state. Section tabs support hover/focus behavior and `Ctrl+1` through `Ctrl+6`. `Ctrl++`, `Ctrl+-`, and `Ctrl+0` change a persisted Qt UI font scale.

The overlay is not the renderer's source of truth. Edits call setters on `PixelToRayWidget`; `syncInfoPanel()` then reads the widget back into controls. `capturePanelState()` creates the persisted representation, and `applyPanelState()` transfers a profile into the widget and lock states.

`QFileSystemWatcher` watches the Current, Preset, and runtime JSON files. Because some writers replace a file atomically, the reload logic re-adds paths after change notifications. A loading/synchronization guard prevents control signals and writes from feeding back recursively.

### `PixelToRayWidget`: scene and interaction owner

`PixelToRayWidget` subclasses `QOpenGLWidget` and `QOpenGLFunctions`. Its state can be divided into:

- local anchor state: latitude, longitude, and roll;
- six signed component lengths, although negative Z is fixed to globe radius by the UI;
- globe radius and a normalized world-orientation quaternion;
- camera eye offset, requested up vector, projection mode, FOV, and orthographic zoom;
- cap angle, image/circle-detection data, visibility toggles, and locks;
- transient drag state and active-handle state; and
- OpenGL programs, textures, buffers, vertex arrays, and view-projection matrix.

Most setters enforce locks, normalize or clamp values, emit status text, and schedule repaint. The widget therefore remains authoritative even when values arrive from Qt controls or reloaded files.

## 6. Coordinate spaces and math

Understanding the coordinate spaces is essential before changing rendering or interaction code.

### Globe-local spherical coordinates

[`FrameMath::latLonToVector()`](../src/FrameMath.cpp#L39) maps latitude `lat`, longitude `lon`, and radius `r` to:

```text
x = r cos(lat) cos(lon)
y = r cos(lat) sin(lon)
z = r sin(lat)
```

At latitude 0 and longitude 0, the anchor is on local +X. At the north pole it is on local +Z.

### Local tangent frame

[`FrameMath::getLocalFrame()`](../src/FrameMath.cpp#L51) builds:

- local Z: radial direction from globe center to anchor;
- initial local X: east tangent;
- initial local Y: north tangent derived by cross product; and
- rolled X/Y axes after applying roll around radial Z.

The six component IDs are positive/negative X, positive/negative Y, and positive/negative Z. The historical misspelling `negtive_*` is a compatibility contract used in C++, JSON, UI labels, and tests.

### Globe orientation and world space

Latitude/longitude/roll define a frame in globe-local space. `m_globeOrientation` then rotates the anchor, local frame, sphere geometry, and globe-axis visuals into world space. Sampling and mouse picking use the conjugate quaternion to transform world directions back into globe-local coordinates.

This separation avoids conflating movement of the anchor over the globe with rotation of the whole globe. It is described in depth in [`013-GLOBE_ORIENTATION_REFACTOR_NOTES.md`](../013-GLOBE_ORIENTATION_REFACTOR_NOTES.md).

### Camera space

The camera target is either world origin (`global_center_fixed=true`) or the current rotated anchor (`false`). The eye position is `target + camera_eye_offset`. The requested up vector is normalized, with fallback axes selected when it is degenerate or parallel to the view direction.

Orthographic projection uses `3 * zoom` as half-height. Perspective projection uses `perspective_fov_y`. After constructing projection and view matrices, [`updateViewProjection()`](../src/PixelToRayWidget.cpp#L1144) applies an NDC translation that centers the projected visible surface bounds.

## 7. Fisheye mapping and geometry generation

### Image loading and disc detection

An image is decoded through `QImage`, converted to RGBA8888, and uploaded to a `QOpenGLTexture` when needed. `detectProjectionCircle()` estimates the circular image border by:

1. averaging small patches in four corners to estimate background color;
2. scanning three central rows and columns for pixels sufficiently different from that background;
3. averaging the left/right/top/bottom bounds; and
4. falling back to a centered circle using the shorter image dimension if detection collapses.

This is heuristic and has no profile-level manual override yet.

### Direction-to-pixel projection

For a world-space point, the code:

1. applies the inverse globe orientation;
2. normalizes the resulting globe-local direction;
3. computes polar angle `theta` from local +Z;
4. rejects directions beyond the hard-coded 140-degree Entaniya cap;
5. computes equisolid normalized radius
   `sin(theta / 2) / sin(capAngle / 2)`;
6. maps local XY direction into the detected image circle; and
7. samples the image.

The sphere's CPU path uses bicubic Catmull-Rom sampling. Some cylinder fallback colors use nearest-neighbor CPU sampling. The active textured-cylinder shader performs the same geometric projection on the GPU and samples a nearest-filtered GL texture.

The profile field `cap_polar_angle` controls visible sphere geometry, while the fisheye sampling cap is currently the independent constant `140.0`. This distinction is easy to miss.

### Generated geometry

All scene meshes are regenerated on each paint:

- textured sphere/cap triangles;
- optional closing cap;
- sphere wireframe;
- global axes and terminal markers;
- y-axis great-circle/panorama band;
- x-tangential cylinder and outline;
- local frame/component lines; and
- selection and origin markers.

When an image is loaded, sphere tessellation increases from 28×48 to 256×512 latitude/longitude bands, making the per-frame CPU generation and sampling path potentially expensive.

### OpenGL passes

[`paintGL()`](../src/PixelToRayWidget.cpp#L729) uses two conceptual passes:

1. surface triangles, optionally with depth test/write; then
2. guide lines and point handles as an always-visible overlay without depth testing.

Wide lines are expanded into screen-facing triangles by `appendLine()` instead of relying only on implementation-dependent OpenGL line widths. The ordinary shader consumes position/color vertices. A separate shader consumes cylinder positions and fallback colors, transforms directions back to globe-local space, and samples the fisheye texture.

## 8. Interaction model

The viewport has four drag modes: none, component length, surface tracking, and global-axis tracking.

| Input | Behavior |
|---|---|
| Left-drag a component endpoint | Projects screen motion onto the component's projected axis and adjusts its clamped length. |
| Left-drag the globe surface | Ray/sphere-picks a new world point, converts it back to globe-local latitude/longitude, and moves the anchor. |
| Shift+left-drag a global endpoint | Computes a quaternion rotating the grabbed world direction to the newly picked sphere direction and applies it to the starting globe orientation. |
| Mouse wheel | Multiplies orthographic zoom and clamps it to 0.35–2.5. |
| `x`, `X`, `y`, `Y`, `z` | Selects signed local components; negative Z is not keyboard-selectable. |
| `w/s`, `a/d`, `q/e` | Adjusts latitude, longitude, and roll. |
| Right-click | Opens viewport visibility/projection toggles. |
| `Esc` | Closes the window. |

Locks are enforced in both direct setters and relevant interaction paths. Any quaternion component lock disables global endpoint dragging as a whole.

## 9. JSON persistence contract

### Files

| File | Meaning |
|---|---|
| `assets/current_state.json` | Mutable Current profile used during normal editing. |
| `assets/preset_state.json` | Preset profile and fallback template. |
| `assets/default_state.json` | Legacy compatibility input; not the preferred current name. |
| `assets/ui_runtime_state.json` | Active Current/Preset mode, current saved-profile name, UI font scale, and image-panel notes. |
| `assets/saved_profiles/**/*.json` | User-named profile snapshots. |

Profiles group fields into visibility, globe orientation, camera, view, local components, image, comment, and locks. Numbers are normalized to at most two decimals when written by the browser server; the Qt side also rounds JSON output through its panel-number helper.

Important conventions:

- quaternions are normalized before use;
- the runtime active-profile values are `current` and `preset`, with legacy `default` accepted by Qt;
- HFC image paths are preferably relative to `assets/hfc-images/`;
- other legacy absolute or `assets/...` image paths are still resolved;
- `profile_comment` is persisted with profiles;
- lock keys use the corresponding field name plus `_locked`; and
- negative Z is derived from globe radius and is not persisted as an editable field.

Because these files are a live integration API, schema changes must be implemented consistently in `PanelState`, Qt load/save/apply/capture code, the browser `SCHEMA`/coercion defaults, saved profiles, and playback actions.

## 10. Browser configuration server

Run the companion server from the repository root:

```bash
python3 tools/config_ui_server.py
```

It binds to `127.0.0.1:8765` by default and tries up to ten subsequent ports if that port is occupied. `--host`, `--port`, and `--assets-dir` can override defaults. The program uses only Python's standard library; `requirements.txt` is empty.

The HTML, CSS, and JavaScript are embedded in the Python file. `ConfigStore` owns validation, path normalization, atomic JSON writes, profile management, file exploration, and HFC-image operations. `ThreadingHTTPServer` serves both the page and API.

### Read endpoints

| Endpoint | Purpose |
|---|---|
| `GET /` | Embedded browser UI. |
| `GET /api/state` | Schema, lock keys, profiles, active profile, profile name, and image notes. |
| `GET /api/schema` | Field and lock schema. |
| `GET /api/active-profile` | Runtime profile selection/name. |
| `GET /api/saved-profiles` | Managed saved-profile listing. |
| `GET /api/files?dir=...` | Image directory explorer. |
| `GET /api/profile-files?dir=...` | JSON profile directory explorer. |
| `GET /api/image?path=...` | Image bytes for preview. |

### Mutation endpoints

| Endpoint | Purpose |
|---|---|
| `POST /api/state/{current|preset}` | Coerced partial profile update. |
| `POST /api/active-profile` | Switch Current/Preset runtime mode. |
| `POST /api/image-panel-key-changes` | Update runtime image-panel notes. |
| `POST /api/saved-profiles/current` | Save Current under a managed name. |
| `POST /api/saved-profiles/current/load` | Load a managed profile as Current. |
| `POST /api/profile-files/current/open` | Open any supported JSON file as Current. |
| `POST .../delete` and `POST .../rename` | Delete or rename a managed saved profile. |
| `POST /api/hfc-images/save` | Copy an image into the HFC domain, optionally overwriting. |
| `POST /api/hfc-images/rename` | Rename an HFC-domain image. |
| `POST /api/hfc-images/delete` | Delete an HFC-domain image. |

The server sends permissive CORS headers. It is intended as a local development tool and has no authentication. Binding it beyond loopback exposes file-browsing and mutation operations, so do that only in a trusted environment.

## 11. Semantic action playback

[`action_playback.py`](../tools/action_playback.py) reads one JSON object per line and submits semantic changes to the browser server. Supported action types are:

- `start_snapshot`: invalidate cached server state;
- `active_profile` / `set_active_profile`: change runtime profile mode;
- `set`: patch one profile key;
- `local_axis_rotation`: post-multiply the stored quaternion by a local X/Y/Z delta;
- `open_profile_file`: replace Current with a JSON profile;
- `image_key_changes`: update the runtime note;
- `wait`: sleep for a requested number of seconds.

Use it while the config server is running:

```bash
python3 tools/action_playback.py tools/action_record_example.jsonl
```

Useful flags:

```bash
python3 tools/action_playback.py tools/action_record_example.jsonl --dry-run
python3 tools/action_playback.py tools/action_record_example.jsonl --timed
python3 tools/action_playback.py log.jsonl --base-url http://127.0.0.1:8766
```

`--timed` honors monotonically interpreted `t` deltas. `--dry-run` validates and prints actions without contacting the server. The player caches `/api/state` and updates that cache after successful mutations.

Automatic recording is not implemented in the current Python tools; the design and proposed hooks are documented in [`018-ACTION_RECORD_PLAYBACK_IMPLEMENTATION_NOTE.md`](../018-ACTION_RECORD_PLAYBACK_IMPLEMENTATION_NOTE.md).

## 12. Tests and validation

### C++

`FrameMathTests` covers spherical mapping, tangent basis construction, roll, signed component direction, vector projection, and length clamping. `InteractionLogicTests` covers hidden/visible handle selection, projected drag distance, and degenerate screen axes.

Run all C++ tests with:

```bash
ctest --test-dir build --output-on-failure
```

Both CTest targets passed during this documentation job.

### Python

The playback tests use a memory client and cover profile patches, active-profile changes, local-axis quaternion order, orientation locks, and JSONL parsing. Because the test imports `action_playback` as a sibling module, run it from `tools/`:

```bash
cd tools
python3 -m unittest action_playback_tests.py
cd ..
```

Alternatively, from the repository root:

```bash
PYTHONPATH=tools python3 -m unittest tools/action_playback_tests.py
```

All five tests passed during this job. Both Python source files also passed `py_compile`, and the example log validated as eight actions under `--dry-run`.

### Current coverage gaps

There are no automated tests for:

- `MainWindow` profile serialization, migration, or file-watcher feedback;
- OpenGL shader compilation and draw output;
- fisheye circle detection or sampling math;
- mouse-to-sphere picking and quaternion globe dragging;
- browser `ConfigStore`, HTTP endpoints, or embedded JavaScript;
- synchronization between a running Qt app and browser server; or
- actual GUI startup in CI/headless environments.

## 13. Developer workflows

### Change math or interactions

Prefer placing pure logic in `pixel_2_ray_core`, add a QtTest case, rebuild, and run CTest. Keeping math outside `QOpenGLWidget` avoids requiring a GL context for unit tests.

### Change a profile field

Update all of these together:

1. `MainWindow::PanelState`;
2. Qt control creation and synchronization;
3. Qt JSON load/save, capture, and apply functions;
4. `PixelToRayWidget` storage/getters/setters/locks if the renderer owns it;
5. browser `SCHEMA`, `LOCK_FIELDS`, defaults, and coercion;
6. representative `assets/*.json` profiles;
7. playback behavior/tests if actions should manipulate it; and
8. user/developer documentation.

### Change rendering

Trace the full pipeline: state → local geometry → globe orientation → camera/view projection → vertex generation → surface or overlay pass. Decide explicitly whether new geometry should participate in depth testing and whether it belongs in the ordinary color shader or textured cylinder shader.

### Change fisheye mapping

Keep CPU sphere sampling and GPU cylinder sampling mathematically aligned. At present they share the conceptual equisolid mapping but use separate implementations and different filtering. Tests should be added before introducing projection models, flips, or manual disc geometry.

### Change browser file operations

Preserve path-domain checks for managed saved profiles and HFC images, use atomic writes/copies where provided, and return specific HTTP status codes for invalid paths, missing files, and conflicts.

## 14. Documentation map

Start with these documents:

- [`001-README.md`](../001-README.md): feature summary and common commands.
- [`009-USER_MANUAL.md`](../009-USER_MANUAL.md): end-user controls and UI behavior.
- [`003-DEVELOPER_WORKFLOW.md`](../003-DEVELOPER_WORKFLOW.md): daily build/test loop.
- [`004-SETUP_AND_TROUBLESHOOTING.md`](../004-SETUP_AND_TROUBLESHOOTING.md): Ubuntu dependencies and setup history.
- [`020-APP_UI_SERVER_COLLABORATION.md`](../020-APP_UI_SERVER_COLLABORATION.md): file-based Qt/browser synchronization.
- [`016-IMAGE_TO_GLOBE_MAPPING_NOTE.md`](../016-IMAGE_TO_GLOBE_MAPPING_NOTE.md): implemented fisheye mapping.
- [`013-GLOBE_ORIENTATION_REFACTOR_NOTES.md`](../013-GLOBE_ORIENTATION_REFACTOR_NOTES.md): coordinate-space and quaternion rationale.
- [`018-ACTION_RECORD_PLAYBACK_IMPLEMENTATION_NOTE.md`](../018-ACTION_RECORD_PLAYBACK_IMPLEMENTATION_NOTE.md): implemented playback and future recorder design.

The remaining root notes fall into three categories:

- focused implementation records: camera pose, fullscreen startup, GLSL compatibility, line width, viewport centering, cylinder depth, and x-tangential cylinder;
- design/refactor plans: projection mapping, cylinder shader texture, HFC image domain, global center, and action recording; and
- historical session/resume material: `002-NEXT_STEPS.md`, `008-SESSION_PROMPTS.md`, and dialog notes.

Read historical plans critically: several contain phased proposals that have since been partly implemented, while older Git branch/commit references may be stale.

## 15. Known risks and maintenance notes

- `assets/` is essential at runtime in this workspace but is untracked. A fresh clone will not reproduce the current profiles and images unless they are supplied separately.
- `requirements.txt` is empty because the Python tools use the standard library; the `.venv/` directory is not required by current code.
- The browser server has no authentication and exposes mutation/file operations. Keep the default loopback binding unless the environment is trusted.
- Sphere geometry and CPU texture sampling are rebuilt at high resolution every frame when an image is loaded.
- CPU and GPU fisheye implementations are duplicated and can drift.
- The fisheye sampling cap is hard-coded to 140 degrees, independent of the configurable visible cap.
- Circle detection is heuristic and has no explicit profile override.
- JSON is the integration protocol but has no version number or formal standalone schema.
- Qt save operations use direct truncate/write, while the Python server uses atomic replacement. Interruption characteristics therefore differ.
- The current tests cover only the small pure-logic core and playback client, not the dominant UI/rendering/server code.
- Existing Markdown contains machine-specific absolute links and stale branch/status statements.
- `src/FrameMath.h` declares `localBandPoint()` without an implementation, but current build sources do not call it.
- Historical backup source files under `src/` can confuse searches; only sources listed in CMake are compiled.

## 16. Recommended onboarding order

For a new contributor:

1. Build and run CTest using the commands in section 4.
2. Read `FrameMath.h/.cpp` and its tests to learn the coordinate convention.
3. Read `PixelToRayWidget.h`, then follow `paintGL()`, `currentFrame()`, `updateViewProjection()`, and the image-sampling functions.
4. Read `MainWindow::PanelState`, startup profile loading, `applyPanelState()`, and JSON load/save.
5. Start the browser server and inspect `/api/state` before reading the embedded front end.
6. Dry-run the example action log, then run it against the server.
7. Consult the focused design notes only for the subsystem being changed.

This order starts with testable coordinate rules, then moves outward to rendering, persistence, browser integration, and automation.
