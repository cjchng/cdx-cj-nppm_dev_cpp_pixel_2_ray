# Pixel To Ray User Manual

## Overview

`pixel_2_ray_qt` is a Qt6 desktop application for visualizing:

- a globe and its mapped fisheye image
- a local coordinate frame attached to the globe surface
- editable signed local component lengths
- globe-fixed global traces and terminal markers
- an optional x-axis tangential cylinder

The application is an interactive viewer and teaching tool. Most values can be changed from the menu-bar `Control Table` overlay, and several viewport operations are available directly by mouse or keyboard.

## Build And Run

From the repository root:

```bash
cmake -S . -B build
cmake --build build
./build/pixel_2_ray_qt
```

Run tests with:

```bash
ctest --test-dir build --output-on-failure
```

## Layout

- Full window: OpenGL viewport
- Overlay: comparison-style `Control Table` with `Current` and `Preset` columns, opened as a left-top dynamic panel by section tabs on the menu bar

Use the section tabs directly after `Commands` on the menu bar to open the matching `Control Table` overlay. Each tab shows its shortcut annotation, such as `Status [Ctrl+1]`. Hovering a tab or pressing its shortcut opens that section and moves keyboard focus into the overlay so the visible controls can be adjusted immediately. A hover/click-opened overlay hides when the pointer leaves the tab/panel area; a shortcut-opened overlay remains focused until the pointer enters and leaves the panel or focus moves elsewhere. The overlay can also be hidden with its `Hide` button. In the Qt overlay, the `Current` and `Preset` column headers are clickable. They switch the Qt application's live runtime state between the saved profile and the preset profile.

## Control Table Overlay

Main sections:

- `Status`
  - active component
  - global center
  - `y-axis great-circle trace`
  - `x-axis tangential cylinder`
  - `Surface Depth Hiding`
  - `Projection`
- `View`
  - latitude
  - longitude
  - roll
  - zoom
  - cap angle
- `Globe Coordinates`
  - globe radius
  - fisheye image
  - anchor `X/Y/Z`
- `Global Orientation`
  - perspective projection toggle, shown first in the browser `Globe Orientation` panel
  - globe quaternion `q_w / q_x / q_y / q_z`
  - per-component locks for `q_w / q_x / q_y / q_z`; locked components reject panel edits, and any active orientation lock pauses shift-drag globe-axis rotation
  - read-only Euler axis-angle equivalent: axis `X/Y/Z` and angle in degrees
  - browser UI server only: transient command rows `Roll (Z) deg`, `Yaw (Y) deg`, and `Pitch (X) deg`; type a numeric delta and press `Enter` to apply one incremental local-axis rotation; these command inputs have no spinner controls and stay disabled while any `Globe q_*` lock is active
  - browser UI server only: the `Euler axis-angle equivalent | read-only` and `Local-axis rotations` subsections have their own `+` / `-` collapse controls inside the `Globe Orientation` panel
  - the command rows compose the delta in the globe-local axis frame, so `Roll (Z) deg` keeps the already-rotated globe-local Z direction fixed while X/Y rotate around it; applying the command still updates the stored quaternion
- `Camera Pose`
  - perspective `FOV Y`
  - eye offsets
  - target `X/Y/Z`
  - up vector
- `Components`
  - `positive_x`
  - `negtive_x`
  - `positive_y`
  - read-only `va`, the angle equivalent of `positive_y` from the global center against local `z`
  - `negtive_y`
  - `positive_z`
  - fixed `negtive_z`

Rows with `Lock` checkboxes block both panel edits and matching interactive edits in the viewport.

## Camera Fields

The Qt overlay labels this section `Camera Pose`. The browser UI server labels the same editable profile section `Camera`.

The camera is applied as:

```text
camera_target = current_scene_center
camera_position = camera_target + camera_eye_offset
view = lookAt(camera_position, camera_target, camera_up)
```

`current_scene_center` depends on `Global Center (Fix/Move)`:

- fixed: target is the global origin `(0, 0, 0)`
- move: target is the current globe anchor

Fields:

- `Perspective FOV Y` / `perspective_fov_y`
  - Vertical field of view in degrees for perspective projection.
  - Clamped to `5..140`.
  - Used when `Perspective Projection` is enabled. Orthographic rendering uses `Zoom` instead, but the FOV value is still saved.
  - Editable in both the Qt overlay and browser UI server.
  - Not lockable.
- `Eye Offset X/Y/Z` / `camera_eye_offset_x/y/z`
  - Offset from the target to the camera eye position.
  - The camera position is not stored separately; it is derived as `target + eye_offset`.
  - The full `X/Y/Z` vector defines the viewing direction from the target to the camera eye.
  - When `Z` is large and `X/Y` is small, the view is nearly vertical. In that case, the `X/Y` pair chooses the horizontal azimuth of that near-vertical view.
  - For example, `(0.01, 0.0, -40.0)` and `(0.0, 0.01, -40.0)` are both near-vertical views, but their horizontal azimuth differs by 90 degrees. Changing between them behaves like rotating the camera eye around the target, not like panning the target.
  - `(0.01, 0.01, -40.0)` gives a diagonal horizontal azimuth.
  - `(0.0, 0.0, -40.0)` is not functional with the default up vector `(0, 0, 1)` because the view direction is parallel to the up vector, so `lookAt(...)` cannot form a stable camera basis.
  - `(0.0, 0.0, 0.0)` is also not functional because the camera eye equals the target and `lookAt(...)` cannot form a view direction.
  - Keep at least one of `Eye Offset X` or `Eye Offset Y` nonzero for vertical-looking views, or use an up vector that is not parallel to the view direction.
  - Editable and lockable in both UIs.
- `Target X/Y/Z`
  - Read-only Qt right-panel rows.
  - Derived from `current_scene_center`.
  - Not shown as editable browser fields and not persisted as separate JSON keys.
- `Up X/Y/Z` / `camera_up_x/y/z`
  - Camera up-vector hint passed to `lookAt(...)`.
  - The renderer normalizes this vector. If it is nearly zero, it falls back to `(0, 0, 1)`.
  - Editable and lockable in both UIs.

### Eye Offset Screenshot Examples

The following screenshots use the same target, projection, and `Eye Offset Z = -40.0`. Only `Eye Offset X` and `Eye Offset Y` change.

Because the camera position is `target + eye_offset`, these examples place the camera far along negative `Z` while using a very small nonzero `X/Y` component to choose the horizontal azimuth of the near-vertical view. The target does not pan. The globe appears to rotate in screen space because the camera eye is being placed at a different azimuth around the same target.

![Eye Offset X 0.01, Y 0.00, Z -40.00](assets/user-manual/camera-eye-offset/eye-offset-x-0_01-y-0_00-z--40.png)

**Image 1: `Eye Offset = (0.01, 0.00, -40.00)`**

The camera eye is slightly offset in positive `X` while staying far along negative `Z`. This produces a valid near-vertical camera direction because the view direction is not exactly parallel to the default up vector. The tiny `X` value chooses the horizontal azimuth used to build the `lookAt(...)` camera basis.

![Eye Offset X 0.00, Y 0.01, Z -40.00](assets/user-manual/camera-eye-offset/eye-offset-x-0_00-y-0_01-z--40.png)

**Image 2: `Eye Offset = (0.00, 0.01, -40.00)`**

The camera eye is now slightly offset in positive `Y` instead of positive `X`. The target and distance are still effectively the same, but the horizontal azimuth changes by 90 degrees. The scene therefore appears rotated compared with Image 1. This is expected: changing `X/Y` changes the camera-eye direction around the target, not the target position.

![Eye Offset X 0.01, Y 0.01, Z -40.00](assets/user-manual/camera-eye-offset/eye-offset-x-0_01-y-0_01-z--40.png)

**Image 3: `Eye Offset = (0.01, 0.01, -40.00)`**

Both horizontal components are nonzero. The camera eye uses a diagonal horizontal azimuth between the Image 1 and Image 2 cases. This gives a valid view and the projected scene appears diagonally rotated.

![Eye Offset X 0.00, Y 0.00, Z -40.00 singular case](assets/user-manual/camera-eye-offset/eye-offset-x-0_00-y-0_00-z--40-singular.png)

**Image 4: `Eye Offset = (0.00, 0.00, -40.00)`**

This is a singular camera pose with the default `Up = (0, 0, 1)`. The camera is directly below the target along the `Z` axis, so the view direction is parallel to the up vector. `lookAt(...)` cannot form a stable right/up/forward basis from parallel vectors, and the viewport can collapse to an unusable view. Keep either `Eye Offset X` or `Eye Offset Y` nonzero for this near-vertical camera setup.

Persisted lock keys:

- `camera_eye_offset_x_locked`
- `camera_eye_offset_y_locked`
- `camera_eye_offset_z_locked`
- `camera_up_x_locked`
- `camera_up_y_locked`
- `camera_up_z_locked`

## Mouse Controls

- Left drag a visible local handle to edit that component length.
- Left click on the globe alone does not move the local origin.
- Left hold-drag on the globe surface moves the local origin only after actual pointer movement.
- `Shift` + left drag a global endpoint rotates the globe.
- Mouse wheel zooms.
- Right click opens the viewport context menu.

## Keyboard Controls

- `x` selects `positive_x`
- `X` selects `negtive_x`
- `y` selects `positive_y`
- `Y` selects `negtive_y`
- `z` selects `positive_z`
- `Z` does not activate `negtive_z`; that axis is fixed
- `w/s` adjust latitude
- `a/d` adjust longitude
- `q/e` adjust roll
- `Ctrl+1` through `Ctrl+6` open and focus the matching menu-bar `Control Table` section tab
- `Ctrl++` or `Ctrl+=` increases Qt UI font size
- `Ctrl+-` decreases Qt UI font size
- `Ctrl+0` resets Qt UI font size
- `ESC` quits

## Commands Menu

The menu bar includes `Commands`:

- `Set Fisheye Image...`
- `Reload Fisheye Image`
- `Clear Fisheye Image`

The menu bar also includes direct section tabs after `Commands`. They open the left-top dynamic `Control Table` overlay for sections such as `Status`, `View`, `Globe Coordinates`, `Global Orientation`, `Camera Pose`, or `Components`. The tab text includes the shortcut legend: `Ctrl+1` for `Status`, `Ctrl+2` for `View`, and so on from left to right. The overlay width scales with the current Qt UI font zoom level so larger fonts get more room. The Qt font zoom level is stored in `assets/ui_runtime_state.json` as `ui_font_scale`, so it persists across app restarts.

The selected fisheye file path is shown in the overlay as a basename, with the full path in the tooltip.

## Viewport Context Menu

Right click in the viewport to toggle:

- `Rear hemisphere wireframe`
- `Local coordinate frame`
- `y-axis great-circle trace`
- `x-axis tangential cylinder`
- `Surface depth hiding`
- `Global center (fix/move)`

## Rendering Notes

- `Surface Depth Hiding`
  - `on`: globe and cylinder surfaces use normal depth test/write
  - `off`: surfaces render in a more x-ray-like view
- `Projection`
  - `orthographic`
  - `perspective`
- The globe surface can be textured from a fisheye image.
- `circular.png` is the built-in default fisheye source unless a saved profile overrides it.
- The globe mapping extracts the valid circular image border and uses that circle for equid-solid projection.
- Globe texture sampling currently uses bicubic interpolation.
- Cylinder surface texturing uses a shader path that inverse-maps each fragment to the fisheye image texture with nearest filtering.

## Geometry Notes

- The default globe radius is `1.0`.
- `negtive_z` is fixed to the globe radius and is not editable.
- The x-axis tangential cylinder radius is:

`globe_radius + positive_z`

- Cylinder heights are:

`positive_y * xCylinderRadius`

`negtive_y * xCylinderRadius`

- The read-only `va` row expresses `positive_y` as the angle at the global center between local `z` and the ray to the `positive_y` endpoint:

`va = atan(positive_y / globe_radius)`

## Global vs Local Visual Language

- Local axes remain the main saturated frame colors.
- Global traces use a separate high-contrast palette.
- Global spokes are thinner and dashed.
- Global positive endpoints use diamond terminal markers.
- Global negative endpoints use `X` terminal markers.
- The local origin uses its own marker shape and does not match endpoint markers.

## Saved Profiles

The application now stores two JSON profiles:

- `preset_state.json`
- `current_state.json`
- `ui_runtime_state.json` for the active `Current` / `Preset` selector and Qt UI font scale

The real files are stored under:

- [assets/preset_state.json](assets/preset_state.json)
- [assets/current_state.json](assets/current_state.json)
- `assets/ui_runtime_state.json`

If those files are missing or empty, the app can import the previous app-data profiles once and then saves the active profiles back to `assets/`.

Behavior:

- editing while `Current` is active updates `current_state.json`
- editing while `Preset` is active updates `preset_state.json`
- the built-in fallback default fisheye path is `assets/hfc-images/circular.png`

## Browser Configuration UI

The Qt `Control Table` overlay is still available. To use the browser companion UI, start:

```bash
python3 tools/config_ui_server.py
```

Then open:

```text
http://127.0.0.1:8765/
```

The browser UI edits the same `Current` and `Preset` profiles in `assets/`. The running Qt app watches those files and reloads external edits. Most configuration sections show Current and Preset side by side as columns in one unified configuration panel. Use the Current/Preset switch in the browser header, or click a browser column header, to choose the active browser profile. Each browser panel title includes a `+` / `-` button to expand or collapse that panel; the collapsed-panel set is remembered in browser `localStorage`. The `Globe Orientation` panel also has local `+` / `-` controls for its Euler axis-angle and local-axis rotation subsections; those collapsed states are remembered in browser `localStorage` and do not change Current or Preset values. The Image panel is active-profile-only and shows only the selected Current or Preset image path.

The Qt application and browser UI synchronize profile values through JSON files. They also synchronize the active `Current` / `Preset` mode through `assets/ui_runtime_state.json`, so switching mode in either UI should switch the other UI. See [020-APP_UI_SERVER_COLLABORATION.md](020-APP_UI_SERVER_COLLABORATION.md).

The browser page title and header include the active `IP:port`, for example `Pixel To Ray Browser Config - 127.0.0.1:8765`.

Use the browser `Profile File Manager` to save a named Current snapshot under `assets/saved_profiles/`, refresh/select profile JSON files, rename the selected profile by editing its filename and pressing Enter, delete a selected profile after a confirmation click, and open a selected profile as Current. The `Save to assets/saved_profiles/` label provides the fixed destination prefix; the editable field contains only the profile subpath below that folder, such as `path/name`. Selecting or renaming a browser row does not rewrite the Save field. The selected browser row is tracked separately by the `Selected browser file` line and by the row highlight, and the selected inline profile editor also uses the subpath-only display. The Profile Files Browser renders entries in two columns on wide screens and one column on narrow screens; the Image file explorer remains single-column. The `Profile Files Browser` row attribute uses `dir` for directories, `profile` for app-managed JSON profiles under `assets/saved_profiles/`, and `json` for regular JSON files outside the managed profile list. `Save` updates the Current snapshot and profile label. The personal comment zone is saved as `profile_comment` in Current and in saved snapshots. `Open`, double-clicking a profile row, clicking a profile file name, or opening any JSON profile file replaces the live Current profile and switches the synchronized active mode to Current. The browser header shows the current profile name tracked in `assets/ui_runtime_state.json`.

If port `8765` is already busy, use another port, for example `python3 tools/config_ui_server.py --port 8766`.

The browser Image panel appears directly below `Current Profile Snapshots`, before `Visibility`, and includes a file explorer backed by the Python server. It follows the active profile and does not show the inactive profile column. The panel starts with the selected image preview, then Save controls, then the HFC Image Browser. The Image browser follows the Profile File Manager pattern: click selects a file, `Open` applies it to the active profile, double-click opens it, `Save` copies the active image into `assets/hfc-images/` after a confirmation click and switches the active profile to the saved relative path, selected `hfc` rows can be renamed inline with Enter, Up/Down changes selection, and `Delete` uses a confirmation click. The Image `OpenDir` directory field keeps the tail of long paths visible so deeply nested filenames/directories are not hidden by the left side of the path. Clicking the `Image` panel title opens a persisted editable `Key Changes` note; edit in the vi-style textarea, then click the title again to save and close. The saved note is restored in the next update phase. Image files inside `assets/hfc-images/` are stored and shown as paths relative to that folder, such as `GOPR0308.JPG` or `subdir/image.jpg`; their first-column row attribute is `hfc`. Use the explorer to open common roots, move up/down directories, preview image files, select a fisheye image path, save it into the HFC image domain, or clear the active profile's image path.

The browser Visibility panel mirrors the viewport right-click toggles, including rear hemisphere wireframe, `World Axes`, local coordinate frame, and cap closed. `World Axes` is a legacy label; the visible overlay is more accurately `Globe Axes`, because the traces and endpoint markers are attached to the oriented globe axes `G_X/G_Y/G_Z`, not to the fixed world frame `W_X/W_Y/W_Z`. When `Local Coordinate Frame` is off, the local axes, local origin marker, component handles, and center-to-local-origin trace are hidden together.

`World Axes` / `Globe Axes` affects both display and interaction. When enabled, the viewport draws the oriented globe-axis great-circle traces, positive/negative endpoint markers, and global-center marker. It also allows shift-drag endpoint picking for globe-orientation tracking. When disabled, those guides are hidden and endpoint picking is disabled. The stored profile key remains `world_axes_visible` for compatibility.

Both the Qt overlay and browser UI show and save numeric configuration fields with at most 2 digits after the decimal point.

In the browser UI, each compact lock toggle is attached to its matching field instead of grouped in a separate section.

## Fullscreen Startup

The app opens on the leftmost available screen. The startup path was adjusted to reduce macOS fullscreen warnings by:

- avoiding normal-window `move(...)` before fullscreen
- removing the extra `window.show()` path
- deferring fullscreen entry until the event loop starts

See [012-FULLSCREEN_STARTUP_NOTE.md](/Users/cj/Documents/cdx-cj-nppm_dev_cpp_pixel_2_ray/012-FULLSCREEN_STARTUP_NOTE.md).

## File Map

- [001-README.md](/Users/cj/Documents/cdx-cj-nppm_dev_cpp_pixel_2_ray/001-README.md): quick repo overview
- [src/main.cpp](/Users/cj/Documents/cdx-cj-nppm_dev_cpp_pixel_2_ray/src/main.cpp): app bootstrap
- [src/MainWindow.cpp](/Users/cj/Documents/cdx-cj-nppm_dev_cpp_pixel_2_ray/src/MainWindow.cpp): window, panel, commands menu, JSON profiles
- [src/PixelToRayWidget.cpp](/Users/cj/Documents/cdx-cj-nppm_dev_cpp_pixel_2_ray/src/PixelToRayWidget.cpp): rendering and viewport interaction
- [src/FrameMath.cpp](/Users/cj/Documents/cdx-cj-nppm_dev_cpp_pixel_2_ray/src/FrameMath.cpp): coordinate and frame math
- [020-APP_UI_SERVER_COLLABORATION.md](020-APP_UI_SERVER_COLLABORATION.md): Qt/browser profile and mode synchronization boundary
- [013-GLOBE_ORIENTATION_REFACTOR_NOTES.md](/Users/cj/Documents/cdx-cj-nppm_dev_cpp_pixel_2_ray/013-GLOBE_ORIENTATION_REFACTOR_NOTES.md): global orientation implementation notes
