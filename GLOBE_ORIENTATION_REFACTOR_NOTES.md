# Globe Orientation Refactor Notes

## Overview [Quaternion Version](https://graphics.cmlab.csie.ntu.edu.tw/~robin/courses/3dcg08/ppt/3dcg08_03trans.pdf)

## Problem Summary

The previous implementation treated globe-axis endpoint dragging as if it should directly update the local frame state:

- `latitude`
- `longitude`
- `roll`

That approach rotates the local `x-y-z` frame only.

It does **not** rotate the globe itself.

This is why the interaction felt wrong.

## Key Distinction

There are three different axis systems in this app:

### World Frame `W_X / W_Y / W_Z`

These are the fixed scene axes.

- `W_X = (1, 0, 0)`
- `W_Y = (0, 1, 0)`
- `W_Z = (0, 0, 1)`

They define the coordinate system in which the camera, OpenGL projection, and true globe center live. They do not rotate with `m_globeOrientation`.

The current `Visibility -> World Axes` UI label does not draw these fixed world axes. It is a legacy label.

### Globe Axes `G_X / G_Y / G_Z`

These are the canonical axes attached to the globe before orientation:

- `G_X = (1, 0, 0)`
- `G_Y = (0, 1, 0)`
- `G_Z = (0, 0, 1)`

They are transformed into world space by the globe orientation quaternion:

```cpp
worldGlobeAxis = m_globeOrientation.rotatedVector(globeAxis);
```

These are what [appendGlobalAxisTraces](/Users/cj/Documents/cdx-cj-nppm_dev_cpp_pixel_2_ray/src/PixelToRayWidget.cpp:1743) currently draws. Therefore the accurate user-facing name is **Globe Axes**, even though the current UI label and JSON key still say `World Axes` / `world_axes_visible`.

### `x-y-z`

These are the local axes attached to the globe anchor point.

They are derived from:

- `latitude`
- `longitude`
- `roll`

in [FrameMath::getLocalFrame](/Users/cj/Documents/cdx-cj-nppm_dev_cpp_pixel_2_ray/src/FrameMath.cpp:54).

So the current naming model is:

- `W_X/W_Y/W_Z` = fixed world frame
- `G_X/G_Y/G_Z` = oriented globe axes at the globe center
- `x/y/z` = local anchor frame on the globe surface

The old `World Axes` label should be read as `Globe Axes` unless a future implementation adds a separate fixed-world-axis overlay.

## Why The Old Approach Was Wrong

The old drag logic updated `latitude/longitude/roll` directly, which means:

- the local frame changed
- the anchor changed
- but the globe geometry itself stayed in the same orientation

That is not “rotate the globe”.

## Refactor Policy

The correct model is:

1. Keep `W_X/W_Y/W_Z` fixed in world space.
2. Introduce an explicit globe orientation state.
3. Rotate the globe geometry and `G_X/G_Y/G_Z` by that state.
4. Rotate the anchor point and local `x-y-z` frame by that same state.
5. Keep `latitude/longitude/roll` as globe-local coordinates, not world coordinates.

## New State

The refactor adds:

```cpp
QQuaternion m_globeOrientation;
```

This quaternion represents the globe-to-world rotation.

## Core Refactor Code

### 1. Rotate the anchor point

Before:

```cpp
QVector3D PixelToRayWidget::currentOrigin() const {
    return FrameMath::latLonToVector(m_state.latitude, m_state.longitude, m_globeRadius);
}
```

After:

```cpp
QVector3D PixelToRayWidget::currentOrigin() const {
    return m_globeOrientation.rotatedVector(
        FrameMath::latLonToVector(m_state.latitude, m_state.longitude, m_globeRadius)
    );
}
```

### 2. Rotate the local frame

Before:

```cpp
FrameMath::Frame PixelToRayWidget::currentFrame() const {
    return FrameMath::getLocalFrame(m_state.latitude, m_state.longitude, m_state.roll);
}
```

After:

```cpp
FrameMath::Frame PixelToRayWidget::currentFrame() const {
    const FrameMath::Frame localFrame =
        FrameMath::getLocalFrame(m_state.latitude, m_state.longitude, m_state.roll);
    return {
        m_globeOrientation.rotatedVector(localFrame.xAxis).normalized(),
        m_globeOrientation.rotatedVector(localFrame.yAxis).normalized(),
        m_globeOrientation.rotatedVector(localFrame.zAxis).normalized()
    };
}
```

### 3. Rotate sphere geometry

Before:

```cpp
const QVector3D p00 = sphericalPoint(m_globeRadius, lat0, lon0);
```

After:

```cpp
const QVector3D p00 =
    m_globeOrientation.rotatedVector(sphericalPoint(m_globeRadius, lat0, lon0));
```

The same rule must be applied to:

- sphere surface
- sphere wireframe
- any other globe-attached geometry that is generated in globe-local coordinates

### 4. Convert world picks back to globe-local coordinates

When tracking the sphere surface, the mouse ray hits a world-space point on the rotated globe.

That point must be converted back into globe-local coordinates before computing `latitude/longitude`:

```cpp
const QVector3D globeLocalPoint =
    m_globeOrientation.conjugated().rotatedVector(spherePoint);
vectorToLatLon(globeLocalPoint, latitude, longitude);
```

## Global Endpoint Drag Policy

Global endpoint drag should rotate the globe, not rewrite the local frame directly.

Correct drag model:

1. User picks a fixed world-space endpoint on the sphere, for example `+X`.
2. Store:
   - the picked world-space direction
   - the starting globe orientation
3. As the user drags, compute the new target world-space sphere direction.
4. Build a delta rotation from the start direction to the target direction.
5. Apply:

```cpp
m_globeOrientation = (delta * startOrientation).normalized();
```

This rotates the whole globe while leaving the global `X-Y-Z` frame fixed.

## Result

After the refactor:

- dragging global endpoints rotates the globe
- the globe center stays fixed
- the global `X-Y-Z` reference stays fixed
- the local `x-y-z` frame follows the rotated globe correctly
- saved/preset mode switching can preserve globe orientation if the quaternion is persisted

## Current Implementation

The Global Orientation module is now a live runtime control path shared by:

- viewport interaction
- the Qt right panel
- `assets/current_state.json`
- `assets/preset_state.json`
- the optional browser configuration server

### Runtime State Owner

The canonical runtime value is:

```cpp
QQuaternion PixelToRayWidget::m_globeOrientation{1.0f, 0.0f, 0.0f, 0.0f};
```

It is exposed through:

```cpp
QQuaternion PixelToRayWidget::globeOrientation() const;
void PixelToRayWidget::setGlobeOrientation(const QQuaternion &orientation);
```

`setGlobeOrientation(...)` normalizes the quaternion, emits status, and requests a repaint. This keeps all UI surfaces and the OpenGL scene tied to the same orientation value.

### Transform Convention

`m_globeOrientation` is the globe-local to world rotation.

Use:

```cpp
m_globeOrientation.rotatedVector(localPoint);
```

when placing globe-attached geometry into world space.

Use:

```cpp
m_globeOrientation.conjugated().rotatedVector(worldPoint);
```

when converting a picked world-space point back into globe-local coordinates.

This convention is used by:

- the globe surface and wireframe
- global endpoint rendering
- texture mapping
- local-frame anchor placement
- local-frame axis placement
- mouse picking on a rotated globe

### Viewport Interaction

Global orientation tracking is intentionally gated by `Shift`.

On `Shift` + left mouse press:

1. `PixelToRayWidget::tryPickGlobalAxisEndpoint(...)` selects a globe-axis endpoint. The function name is legacy.
2. The drag mode becomes `DragState::Mode::GlobalAxisTrack`.
3. The widget stores:
   - `m_drag.startOrientation`
   - `m_drag.grabbedWorldPoint`
   - `m_drag.preferRearHemisphere`

During drag:

```cpp
const QQuaternion delta = QQuaternion::rotationTo(startDirection, targetDirection);
m_globeOrientation = (delta * m_drag.startOrientation).normalized();
```

This rotates the globe without rewriting `latitude`, `longitude`, or `roll`. The local frame still follows because it is derived in globe-local coordinates and then rotated into world space.

### Qt Right Panel

The right panel exposes the quaternion in the `Global Orientation` section:

- `Globe q_w`
- `Globe q_x`
- `Globe q_y`
- `Globe q_z`

Each field is connected to `PixelToRayWidget::setGlobeOrientation(...)`.

Below the quaternion rows, the panel shows a high-contrast delimiter followed by a read-only 4-component Euler axis-angle equivalent:

- `Euler axis_x`
- `Euler axis_y`
- `Euler axis_z`
- `Euler angle deg`

The axis-angle values are computed by `axisAngleFromQuaternion(...)` in `MainWindow.cpp` during `syncInfoPanel()`. They are not stored separately. They are derived from the normalized quaternion and update whenever:

- quaternion spin boxes are edited
- global endpoint tracking changes the orientation
- saved/preset mode is toggled
- browser edits are reloaded from the JSON profiles

For identity orientation, the display uses axis `(0, 0, 1)` and angle `0`.

The UI and saved numeric profile values are rounded to at most 2 digits after the decimal point. The quaternion is normalized again when applied to the widget.

### Quaternion To Axis-Angle Transfer

The right panel stores and edits global orientation as a quaternion:

```text
Global q_w = w
Global q_x = x
Global q_y = y
Global q_z = z
```

Qt calls the scalar component `q_w` through `QQuaternion::scalar()`. The vector part is `(q_x, q_y, q_z)`.

The equivalent four-component axis-angle display is:

```text
Euler axis_x
Euler axis_y
Euler axis_z
Euler angle deg
```

The `Euler axis_*` label means the four-value axis-angle form of the same global quaternion. It is not a yaw/pitch/roll triplet; it is one rotation axis plus one rotation amount.

The conversion used in both `MainWindow.cpp` and `tools/config_ui_server.py` is:

1. Normalize the quaternion.

   ```text
   length = sqrt(w*w + x*x + y*y + z*z)

   w = w / length
   x = x / length
   y = y / length
   z = z / length
   ```

2. Prefer the positive-scalar spelling of the same rotation.

   ```text
   if w < 0:
       w = -w
       x = -x
       y = -y
       z = -z
   ```

   Quaternions `q` and `-q` encode the same physical rotation. Flipping to `w >= 0` keeps the displayed angle in the shorter `0..180` degree range and avoids sudden sign changes in the displayed axis.

3. Compute the vector-part length.

   ```text
   v = sqrt(x*x + y*y + z*z)
   ```

4. If `v` is almost zero, treat it as identity rotation.

   ```text
   axis = (0, 0, 1)
   angle = 0 deg
   ```

   The axis is arbitrary when the angle is zero. [The implementation chooses positive Z as a stable display convention.](https://www.khronos.org/opengl/wiki/Calculating_a_Quaternion_from_a_Rotation#From_Quaternion_to_Axis-Angle) This also avoids division by zero in the next step.

5. Otherwise, normalize the vector part to get the rotation axis.

   ```text
   axis_x = x / v
   axis_y = y / v
   axis_z = z / v
   ```

6. Compute the rotation angle.

   ```text
   angle_rad = 2 * atan2(v, w)
   angle_deg = angle_rad * 180 / pi
   ```

So, in compact form:

```text
axis = normalize((q_x, q_y, q_z))
angle = 2 * atan2(length((q_x, q_y, q_z)), q_w)
```

after quaternion normalization and optional sign flip.

The inverse relation is:

```text
q_w = cos(angle / 2)
q_x = axis_x * sin(angle / 2)
q_y = axis_y * sin(angle / 2)
q_z = axis_z * sin(angle / 2)
```

where `angle` is in radians and `axis` is normalized.

Example:

```text
q = (0.7071, 0.0, 0.0, 0.7071)
axis = (0.0, 0.0, 1.0)
angle = 90 deg
```

That means a 90 degree rotation around the global Z axis.

### Relationship To Roll-Yaw-Pitch

Roll-yaw-pitch is another way to describe orientation, but it is a different representation from axis-angle.

Axis-angle says:

```text
rotate angle degrees around this one 3D axis
```

Roll-yaw-pitch says:

```text
apply three ordered rotations around named axes
```

The order matters. For example, `roll -> pitch -> yaw` and `yaw -> pitch -> roll` are generally different rotations even if the same three numbers are used.

A common aerospace convention is:

```text
yaw   = rotation around Z
pitch = rotation around Y
roll  = rotation around X
```

One common quaternion composition for that convention is:

```text
q = q_yaw_z * q_pitch_y * q_roll_x
```

but this is only correct for that chosen axis convention and multiplication order. Other libraries or camera systems may use a different order, handedness, or intrinsic/extrinsic convention.

The current app intentionally does not store global orientation as roll-yaw-pitch because:

- quaternions compose cleanly during endpoint dragging
- quaternions avoid gimbal lock
- the same quaternion can rotate vectors directly through `QQuaternion::rotatedVector(...)`
- the axis-angle display gives a compact 4-component human-readable equivalent

Important distinction in this repo:

- `roll` in the `View` section belongs to the local `x-y-z` frame.
- `Global q_w/q_x/q_y/q_z` belongs to the whole globe orientation.
- `Euler axis_x/axis_y/axis_z/angle` is a read-only equivalent of `Global q_*`.
- roll-yaw-pitch is not currently persisted for global orientation.

If a future UI needs roll-yaw-pitch controls, it should define the exact convention before implementation:

- axis order, for example `Z-Y-X`
- intrinsic or extrinsic rotations
- degrees or radians
- right-handed or left-handed interpretation
- how to handle gimbal-lock singularities near pitch `+/-90 deg`

### JSON Profile Keys

Global orientation is persisted in both profile files with these keys:

```json
{
  "globe_orientation_scalar": 1.0,
  "globe_orientation_x": 0.0,
  "globe_orientation_y": 0.0,
  "globe_orientation_z": 0.0
}
```

The profile files live under:

- `assets/current_state.json`
- `assets/preset_state.json`

On startup, `MainWindow` loads Preset first, then Current. It can still import the older app-data profile location as a fallback, then saves the active profiles back into `assets/`.

### Browser Configuration UI

`tools/config_ui_server.py` exposes the same quaternion fields in its `Globe Orientation` section.

The browser UI also displays a read-only axis-angle equivalent below a contrast delimiter:

- `Euler axis_x`
- `Euler axis_y`
- `Euler axis_z`
- `Euler angle deg`

The derived axis-angle values are computed client-side in JavaScript by `axisAngleFromState(...)`. They update immediately while quaternion inputs change, before the next server save.

The derived axis-angle values are not written into JSON. The quaternion keys remain the only persisted orientation representation.

### Browser Incremental Rotation Axis Frame

The browser-only `Roll (Z) deg`, `Yaw (Y) deg`, and `Pitch (X) deg` rows are transient command inputs. They are not stored as separate profile values.

The command implementation composes the entered delta on the right side of the current quaternion:

```text
q_new = q_current * q_delta_axis
```

This means the delta is interpreted in the globe-local/body axis frame. For example, `Roll (Z) deg` is applied around the already-rotated local Z axis, so the displayed local Z direction remains fixed while the other local axes rotate around it. The same principle applies to `Yaw (Y) deg` and `Pitch (X) deg`.

The previous implementation used left-side composition:

```text
q_new = q_delta_axis * q_current
```

That interpreted the delta in the world/global axis frame. Under that old order, `Roll (Z) deg` rotated around fixed world Z, so the visible globe-local Z axis could move when it was already tilted away from world Z.

Keeping the rotated axis fixed does not mean keeping `q` fixed. The quaternion is the stored orientation itself, so any visible rotation must produce a new quaternion. The fixed-axis property is a vector invariant:

```text
q_new.rotate(local_z) == q_current.rotate(local_z)
```

for a local-Z roll. If `q_new == q_current`, no orientation change happened.

### Fixed Global Center Screen Anchor

When `Global Center` is fixed, the viewport explicitly recenters the projected global center after building the camera projection:

```text
clip_center = MVP * (0, 0, 0, 1)
MVP = screen_center_correction * MVP
```

The correction translates by the negative projected `x/y` NDC offset of the global center. This keeps the global center guide at the screen center without changing the saved camera target, camera eye offset, or profile values.

### Cross-UI Synchronization

The Qt panel saves the active profile through `saveStateFiles()`.

The browser server writes the same `assets/*.json` files.

`MainWindow` uses `QFileSystemWatcher` to watch:

- `assets/current_state.json`
- `assets/preset_state.json`

When either file changes, `reloadExternalStateFiles()` reloads both profiles and reapplies the active mode to the widget. This is what lets browser-side orientation edits update the running Qt scene and right panel.

Profile values synchronize through `assets/current_state.json` and `assets/preset_state.json`. Active `Current` / `Preset` mode synchronizes separately through `assets/ui_runtime_state.json`. See `APP_UI_SERVER_COLLABORATION.md`.

### Implementation Boundaries

- `latitude`, `longitude`, and `roll` remain local-frame parameters.
- `m_globeOrientation` is the globe rotation parameter.
- The axis-angle rows are display-only.
- The term `Euler axis-angle` here means the 4-component axis-angle representation of the same quaternion. It is not a yaw/pitch/roll triplet.

## Supplement: Current Implementation Snapshot

This supplement describes the current implementation only.

### State Model

Global orientation is stored as a normalized quaternion in `PixelToRayWidget`:

```cpp
QQuaternion m_globeOrientation{1.0f, 0.0f, 0.0f, 0.0f};
```

The quaternion components are:

- `scalar()` = `q_w`
- `x()` = `q_x`
- `y()` = `q_y`
- `z()` = `q_z`

The quaternion represents the globe-local to world rotation. Globe-attached points are transformed into world space with:

```cpp
m_globeOrientation.rotatedVector(localPoint)
```

World-space picks on the rotated globe are converted back to globe-local space with:

```cpp
m_globeOrientation.conjugated().rotatedVector(worldPoint)
```

### Stored Profile Keys

The profile JSON stores the quaternion components directly:

```json
{
  "globe_orientation_scalar": 1.0,
  "globe_orientation_x": 0.0,
  "globe_orientation_y": 0.0,
  "globe_orientation_z": 0.0
}
```

The same profile also stores per-component lock state:

```json
{
  "globe_orientation_scalar_locked": false,
  "globe_orientation_x_locked": false,
  "globe_orientation_y_locked": false,
  "globe_orientation_z_locked": false
}
```

These keys are used by both:

- `assets/current_state.json`
- `assets/preset_state.json`

### Qt Right Panel

The right panel exposes the editable quaternion rows in the `Global Orientation` panel:

- `Globe q_w`
- `Globe q_x`
- `Globe q_y`
- `Globe q_z`

Each row has a lock control. A locked row disables its numeric editor and preserves that component when `setGlobeOrientation(...)` receives a new quaternion.

Below the quaternion rows, the panel displays a read-only axis-angle representation:

- `Euler axis_x`
- `Euler axis_y`
- `Euler axis_z`
- `Euler angle deg`

These four values are derived during `syncInfoPanel()`. They are not stored in JSON.

### Browser UI Server

`tools/config_ui_server.py` exposes the same quaternion rows in the browser `Globe Orientation` panel and attaches lock controls through `LOCK_FIELDS` / `LOCK_KEYS`.

The browser computes the read-only axis-angle display client-side from the current quaternion state. Only the quaternion and lock keys are persisted.

### Lock Behavior

`PixelToRayWidget::setGlobeOrientation(...)` applies lock state per component:

- locked components keep the current runtime value
- unlocked components accept the requested value
- the resulting quaternion is normalized
- an invalid near-zero quaternion is ignored

If any globe orientation component is locked, shift-drag globe-axis tracking is disabled for that interaction. This prevents viewport rotation from changing locked quaternion components indirectly.

Profile application temporarily clears the locks, applies the stored quaternion, and then restores the stored lock states. This allows profile loading and Current/Preset switching to apply the saved orientation even when a profile contains locked components.

### Synchronization

The Qt app and browser UI synchronize through the files in `assets/`:

- `current_state.json` carries the Current profile values
- `preset_state.json` carries the Preset profile values
- `ui_runtime_state.json` carries the active Current/Preset mode

`MainWindow` watches the profile files with `QFileSystemWatcher`. When the browser server writes a profile file, the Qt app reloads and reapplies the active profile. When the right panel changes values, the app saves the active profile back to the same files.

### Derived Axis-Angle Display

The displayed axis-angle values are computed from the normalized quaternion:

```text
axis = normalize((q_x, q_y, q_z))
angle = 2 * atan2(length((q_x, q_y, q_z)), q_w)
```

For identity orientation, the display uses:

```text
axis = (0, 0, 1)
angle = 0 deg
```

The axis-angle rows are a 4-component equivalent of the quaternion. They are not yaw, pitch, and roll controls.

## Files Affected

- [src/PixelToRayWidget.h](/Users/cj/Documents/cdx-cj-nppm_dev_cpp_pixel_2_ray/src/PixelToRayWidget.h)
- [src/PixelToRayWidget.cpp](/Users/cj/Documents/cdx-cj-nppm_dev_cpp_pixel_2_ray/src/PixelToRayWidget.cpp)
- [src/MainWindow.h](/Users/cj/Documents/cdx-cj-nppm_dev_cpp_pixel_2_ray/src/MainWindow.h)
- [src/MainWindow.cpp](/Users/cj/Documents/cdx-cj-nppm_dev_cpp_pixel_2_ray/src/MainWindow.cpp)
- [tools/config_ui_server.py](/Users/cj/Documents/cdx-cj-nppm_dev_cpp_pixel_2_ray/tools/config_ui_server.py)
