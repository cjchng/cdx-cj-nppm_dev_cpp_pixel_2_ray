# Pixel To Ray Qt Rewrite

This repository contains a C++/Qt Widgets rewrite of the original browser-based sphere-frame demo.

## Current Features

- `QMainWindow` shell with a full-window viewport and left-top dynamic `Control Table` overlay opened by hoverable/focusable menu-bar section tabs
- `QOpenGLWidget` viewport for the globe, local frame, global traces, and cylinder
- keyboard, drag, wheel, and right-click controls
- persistent Chrome-style Qt UI font zoom with `Ctrl++`, `Ctrl+-`, and `Ctrl+0`
- menu-bar control section shortcuts with visible tab legends, `Ctrl+1` through `Ctrl+6`, plus auto-focus and auto-hide overlay behavior
- globe texture mapping from a selectable fisheye image
- saved `current` and `preset` state profiles in JSON
- command menu for selecting, reloading, and clearing the fisheye image

## Build

You need a C++17 compiler, CMake, and Qt6 development packages.

- Setup details: [SETUP_AND_TROUBLESHOOTING.md](/Users/cj/Documents/cdx-cj-nppm_dev_cpp_pixel_2_ray/SETUP_AND_TROUBLESHOOTING.md)
- Normal edit/build/test loop: [DEVELOPER_WORKFLOW.md](/Users/cj/Documents/cdx-cj-nppm_dev_cpp_pixel_2_ray/DEVELOPER_WORKFLOW.md)

```bash
cmake -S . -B build
cmake --build build
./build/pixel_2_ray_qt
```

To run tests:

```bash
ctest --test-dir build --output-on-failure
```

## Controls

- Left drag a local handle to edit its length
- Left hold-drag on the globe surface to move the local origin on the globe
- `Shift` + left drag a global endpoint to rotate the globe
- Mouse wheel to zoom
- Right click for viewport toggles
- `x/X`, `y/Y`, `z/Z` to select signed local components
- `w/s`, `a/d`, `q/e` to adjust latitude, longitude, and roll
- `ESC` to quit

## State Files

The app now stores its profiles as JSON, not `QSettings`.

- `preset_state.json`
- `current_state.json`

The real files are stored under:

- `assets/preset_state.json`
- `assets/current_state.json`

If the assets files are missing or empty, the app can import the old app-data profiles once and then save them to `assets/`.

## Browser Configuration UI

The Qt `Control Table` overlay remains the primary in-app control surface. Hover a menu-bar section tab after `Commands` to open the desired left-top overlay section, or press the shortcut shown on the tab label. The overlay focuses its first available control and scales its width with the current UI font zoom. Hover/click-opened overlays hide when the pointer leaves the tab/panel area; shortcut-opened overlays stay focused until the pointer enters and leaves the panel or focus moves elsewhere. The overlay can also be hidden with the `Hide` button. A browser-based companion UI can edit the same JSON profiles:

```bash
python3 tools/config_ui_server.py
```

Open:

```text
http://127.0.0.1:8765/
```

The browser UI reads and writes `assets/current_state.json` and `assets/preset_state.json`. The running Qt app watches those files and reloads external edits. Most configuration sections show Current and Preset side by side as columns in one unified panel. Use the Current/Preset switch in the browser header, or click a browser column header, to choose the active browser profile. Each browser panel has a `+` / `-` title control for dynamic collapse/expand, remembered in browser `localStorage`. The `Globe Orientation` panel also has local `+` / `-` controls for the Euler axis-angle equivalent and local-axis rotation subsections; those collapsed states are stored only in browser `localStorage`. The Image panel is active-profile-only and shows only the selected Current or Preset image path.

The Qt application and browser UI synchronize profile values through JSON files. They also synchronize the active `Current` / `Preset` mode through `assets/ui_runtime_state.json`, and Qt stores the persistent UI font scale in the same runtime file as `ui_font_scale`. See [APP_UI_SERVER_COLLABORATION.md](APP_UI_SERVER_COLLABORATION.md).

The browser page title and header include the active `IP:port`, for example `Pixel To Ray Browser Config - 127.0.0.1:8765`.

Use the browser `Profile File Manager` to save `assets/current_state.json` under `assets/saved_profiles/` with `Save`, refresh/select profile JSON files, rename a selected profile by editing its filename and pressing Enter, delete selected profiles with a confirmation click, and replace the live Current profile with `Open`, by clicking a profile file name, or by opening any JSON profile file. The `Save to assets/saved_profiles/` label provides the fixed destination prefix; the editable field contains only the profile subpath below that folder, such as `path/name`. Selecting or renaming a browser row does not rewrite the Save field. The selected inline profile editor uses the same subpath-only display. The Profile Files Browser renders entries in two columns on wide screens and one column on narrow screens; the Image file explorer remains single-column. The `Profile Files Browser` row attribute uses `dir` for directories, `profile` for app-managed JSON profiles under `assets/saved_profiles/`, and `json` for regular JSON files outside the managed profile list. The editable personal comment is saved as `profile_comment` in the Current profile and copied into saved snapshots. The browser header labels the current profile name stored in `assets/ui_runtime_state.json`.

If port `8765` is already busy, start the server with another port, for example `python3 tools/config_ui_server.py --port 8766`.

The browser Image panel appears directly below `Current Profile Snapshots`, before `Visibility`, and includes a server-side file explorer for selecting supported fisheye images from `assets/hfc-images`, `assets/testing-images`, `assets`, the repository, or the user home directory. It follows the active profile and does not show the inactive profile column. The panel starts with the selected image preview, then Save controls, then the HFC Image Browser. The Image browser follows the Profile File Manager pattern: click selects a file, `Open` applies it to the active profile, double-click opens it, `Save` copies the active image into `assets/hfc-images/` after a confirmation click and switches the active profile to the saved relative path, selected `hfc` rows can be renamed inline with Enter, and `Delete` uses a confirmation click. The `OpenDir` directory field scrolls to the tail of long paths so the rightmost directory/file context remains visible. Clicking the `Image` panel title opens a persisted editable `Key Changes` note with a lightweight vi-style mode line; clicking the title again saves the note and closes it. Images inside `assets/hfc-images/` are stored and shown as paths relative to that folder, such as `GOPR0308.JPG` or `subdir/image.jpg`; their first-column row attribute is `hfc`.

The browser Visibility panel includes the viewport right-click toggles: rear hemisphere wireframe, `World Axes`/globe axes, local coordinate frame, y-axis great-circle trace, x-axis tangential cylinder, surface depth hiding, cap closed, and global center fix/move. The `World Axes` label is legacy; the overlay follows the oriented globe axes.

Compact lock toggle buttons are shown inline with their matching fields in the browser UI to keep the configuration page compact without narrowing numeric inputs.

## Notes

- `assets/hfc-images/circular.png` is the built-in default fisheye image when no saved/preset profile overrides it.
- Numeric configuration fields are displayed and saved with at most 2 digits after the decimal point.
- Qt/browser collaboration and Current/Preset mode synchronization are documented in `APP_UI_SERVER_COLLABORATION.md`.
- Global orientation shows quaternion controls plus collapsible Euler axis-angle and local-axis rotation subsections in the browser UI.
- Fisheye image-to-globe mapping and the next projection/ray-map/flip plan are documented in `IMAGE_TO_GLOBE_MAPPING_NOTE.md` and `FISHEYE_GLOBE_MAPPING_PLAN.md`.
- Semantic action playback is documented in `ACTION_RECORD_PLAYBACK_PLAN.md`; implementation details and the `tools/action*.py` dissection are in `ACTION_RECORD_PLAYBACK_IMPLEMENTATION_NOTE.md`.
- Current global orientation implementation details are documented in `GLOBE_ORIENTATION_REFACTOR_NOTES.md`.
- The `Pixel To Ray Qt Window` opens on the leftmost available screen at startup.
- `Global center (fix/move)` changes camera targeting, not the physical sphere center.
- Global traces now use a separate palette and dashed/terminal-marker styling so they read differently from the local frame.
- The historical spelling `negtive_*` is intentionally preserved in state keys and UI labels.
