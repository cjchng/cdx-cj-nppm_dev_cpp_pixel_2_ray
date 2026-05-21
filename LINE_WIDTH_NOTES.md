# Line Width Notes

## Where To Adjust Line Width

The contour thickness is controlled in [src/PixelToRayWidget.cpp](/Users/cj/Documents/cdx-cj-nppm_dev_cpp_pixel_2_ray/src/PixelToRayWidget.cpp:21):

```cpp
constexpr float kContourLineWidth = 4.0f;
constexpr float kActiveContourLineWidth = 5.5f;
```

- `kContourLineWidth`: normal sphere/frame contour thickness in screen pixels
- `kActiveContourLineWidth`: highlighted active component thickness in screen pixels

If you want thicker or thinner strokes, change those two constants and rebuild:

```bash
cmake --build build
```

## Why The First Approach Did Not Work Well

The original renderer drew contours with OpenGL `GL_LINES` and relied on:

```cpp
glLineWidth(lineWidth);
glDrawArrays(GL_LINES, ...)
```

That approach is not reliable on modern macOS OpenGL drivers.

### Root Cause

- Qt is creating a modern OpenGL context for the widget.
- On macOS, wide `GL_LINES` are effectively unsupported or heavily clamped.
- In practice, the driver often behaves as if the maximum line width is near `1.0`, even if the code requests `6.4f` or `8.4f`.

So this code:

```cpp
constexpr float kContourLineWidth = 6.4f;
constexpr float kActiveContourLineWidth = 8.4f;
```

could compile and run correctly but still produce nearly unchanged visual output.

## What Changed

The renderer now builds each line segment as explicit triangle geometry instead of depending on `GL_LINES`.

Current behavior:

- each logical line segment is projected to screen space
- a perpendicular screen-space offset is computed
- that offset is converted back into world-space width for the current orthographic camera
- two triangles are emitted to form a visible thick stroke

This implementation lives in [src/PixelToRayWidget.cpp](/Users/cj/Documents/cdx-cj-nppm_dev_cpp_pixel_2_ray/src/PixelToRayWidget.cpp:367) inside `appendLine(...)`.

## Why The New Method Works Better

- thickness is now geometry-driven instead of driver-dependent
- the width constants map to visible pixel thickness much more directly
- the result is stable on macOS where `glLineWidth()` is not trustworthy

## Relevant Render Path

- [src/PixelToRayWidget.cpp](/Users/cj/Documents/cdx-cj-nppm_dev_cpp_pixel_2_ray/src/PixelToRayWidget.cpp:126): `paintGL()`
- [src/PixelToRayWidget.cpp](/Users/cj/Documents/cdx-cj-nppm_dev_cpp_pixel_2_ray/src/PixelToRayWidget.cpp:367): `appendLine(...)`
- [src/PixelToRayWidget.cpp](/Users/cj/Documents/cdx-cj-nppm_dev_cpp_pixel_2_ray/src/PixelToRayWidget.cpp:418): sphere wireframe generation
- [src/PixelToRayWidget.cpp](/Users/cj/Documents/cdx-cj-nppm_dev_cpp_pixel_2_ray/src/PixelToRayWidget.cpp:515): frame and active-axis geometry

## Practical Guidance

- For a subtle look, try `4.0f` and `5.5f`
- For the current bold look, keep `6.4f` and `8.4f`
- If very large values start to look blunt at line joins, that is expected because each segment is currently expanded independently
