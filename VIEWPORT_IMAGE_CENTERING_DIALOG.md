# Viewport Image Centering Dialog

## Dialog Summary

The user provided a screenshot of the Qt application and requested:

> Adjust the image to center of the screen

The screenshot showed the rendered fisheye/globe/cylinder scene visually displaced from the center of the left OpenGL viewport. The right-side control table remained in place; the request was interpreted as a viewport rendering issue, not a browser UI server layout issue.

## Context Observed

The active view used perspective projection and a camera configuration close to a numerically fragile `lookAt(...)` case:

- `Eye Offset Z` was large and positive.
- `Camera Up Z` was `1`.
- The camera direction could become nearly parallel or anti-parallel to the requested up vector.

The existing code already had a limited center correction in `PixelToRayWidget::updateViewProjection()`, but it only corrected the projected global origin when `Global Center` was fixed. That did not guarantee that the visible rendered image bounds were centered in the viewport.

## Implementation Decision

The correction was made in the Qt OpenGL camera/projection path.

The implementation does not modify saved profile values, quaternion values, or UI server APIs. It changes how the existing scene is projected onto the viewport.

Two changes were made:

- Add a stable camera-up fallback when the requested up vector is nearly parallel to the view direction.
- Center the projected visual bounds of the rendered surface instead of only centering one mathematical point.

## Current Implementation

`PixelToRayWidget::cameraUpForView()` resolves the up vector used by `QMatrix4x4::lookAt(...)`.

If the requested up vector is usable, the app keeps it. If it is degenerate for the current view direction, the app chooses a fallback world axis that is not parallel to the view direction.

`PixelToRayWidget::projectedSurfaceBoundsCenterNdc(...)` samples the visible globe surface and, when enabled, the x-tangent cylinder. It projects those sampled points into normalized device coordinates and computes the projected center of the visible bounds.

`PixelToRayWidget::updateViewProjection()` then applies a screen-space correction so those projected bounds are centered in the viewport.

The thick-line guide code also uses the resolved camera-up basis, so guide offsets and rendered surface centering use the same view basis.

## Files Changed

- `src/PixelToRayWidget.h`
- `src/PixelToRayWidget.cpp`

Key functions:

- `cameraUpForView()`
- `projectedSurfaceBoundsCenterNdc(...)`
- `updateViewProjection()`
- `appendLine(...)`
- `screenSpaceOffsetAtPoint(...)`

## Verification

The implementation was verified with:

```bash
cmake --build build
ctest --test-dir build --output-on-failure
git diff --check src/PixelToRayWidget.cpp src/PixelToRayWidget.h
```

All tests passed and the diff check was clean.

## Operational Note

The change affects the rebuilt Qt executable. A currently running Qt window must be restarted to show the updated projection behavior.
