# Camera Pose Notes

## Previous State

Before the camera-pose panel change, the viewport camera was mostly fixed in code:

- `cameraPosition()` returned `currentSceneCenter() + QVector3D(4.0f, 4.0f, 4.0f)`
- `lookAt(...)` always used:
  - eye: `cameraPosition()`
  - target: `currentSceneCenter()`
  - up: `QVector3D(0.0f, 0.0f, 1.0f)`

That meant the user could only influence the view through:

- `Zoom`
- `Global center (fix/move)`

There was no right-panel UI for actual camera pose values.

## Implemented Policy

The current implementation exposes the camera pose on the right panel in a way that preserves the existing `Global center` behavior.

Editable fields:

- `Eye Offset X`
- `Eye Offset Y`
- `Eye Offset Z`
- `Up X`
- `Up Y`
- `Up Z`

Read-only fields:

- `Target X`
- `Target Y`
- `Target Z`

## Why Target Is Read-Only

The app already uses `Global center (fix/move)` as the policy for the `lookAt(...)` target:

- `fix`: target is world origin
- `move`: target is the current sphere anchor

Keeping the target read-only avoids conflicting camera-control models on the same screen.

So the current camera model is:

- `cameraTarget()` = `currentSceneCenter()`
- `cameraPosition()` = `cameraTarget() + cameraEyeOffset`
- `cameraUp()` = normalized user-controlled up vector

## Result

The user can now adjust:

- camera position relative to the active target
- camera roll/orientation basis through the up vector

while still using `Global center (fix/move)` to decide what the camera looks at.

## Files

- [src/PixelToRayWidget.h](/Users/cj/Documents/cdx-cj-nppm_dev_cpp_pixel_2_ray/src/PixelToRayWidget.h)
- [src/PixelToRayWidget.cpp](/Users/cj/Documents/cdx-cj-nppm_dev_cpp_pixel_2_ray/src/PixelToRayWidget.cpp)
- [src/MainWindow.h](/Users/cj/Documents/cdx-cj-nppm_dev_cpp_pixel_2_ray/src/MainWindow.h)
- [src/MainWindow.cpp](/Users/cj/Documents/cdx-cj-nppm_dev_cpp_pixel_2_ray/src/MainWindow.cpp)
