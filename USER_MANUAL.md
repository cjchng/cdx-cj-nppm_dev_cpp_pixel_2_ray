# Pixel To Ray User Manual

## Overview

`pixel_2_ray_qt` is a Qt6 desktop application that visualizes a point on a sphere, its local coordinate frame, and six signed axis-aligned component handles:

- `positive_x`
- `negtive_x`
- `positive_y`
- `negtive_y`
- `positive_z`
- `negtive_z`

The application is intended as an interactive viewer. You can change the active component, drag editable handle lengths, drag directly on the globe to reposition the local frame, and toggle several rendering options.

## System Requirements

- CMake 3.16 or newer
- A C++17 compiler
- Qt6 development packages with these modules:
  - `Widgets`
  - `Gui`
  - `Test`
  - `OpenGL`
  - `OpenGLWidgets`
- A machine and driver stack that can create a working OpenGL context

## Build

From the repository root:

```bash
cmake -S . -B build
cmake --build build
```

This produces the desktop binary at:

`build/pixel_2_ray_qt`

## Run

Start the application from the repository root:

```bash
./build/pixel_2_ray_qt
```

When the window opens:

- The left side is the OpenGL viewport.
- The right side is a live status panel.
- The status panel shows the active component, latitude, longitude, roll, zoom, rendering toggles, and all six component lengths.
- Each status line includes its related input hint.

## Running Tests

From the repository root:

```bash
ctest --test-dir build --output-on-failure
```

The current test suite covers:

- spherical coordinate conversion
- local frame generation
- roll application
- signed component direction lookup
- drag projection math
- basic picking behavior

## Interface Guide

### Viewport

The viewport renders:

- a shaded sphere
- sphere wireframe lines
- the local coordinate frame at the current sphere anchor point
- editable component handles plus one fixed inward `negtive_z` segment
- optional Y-axis great-circle trace
- optional X-axis tangential cylinder
- a highlight for the currently active component

### Status Panel

The right panel is read-only. It mirrors the current application state and shows inline control hints next to the live values.

## Controls

### Mouse

- Left click a visible handle to select it.
- Left drag after selecting a handle to change that component's length.
- Left click and drag on the globe surface, away from handles, to move the local frame over the sphere.
- Mouse wheel zooms in and out.
- Right click opens the context menu.

### Keyboard

- `x` selects `positive_x`
- `X` selects `negtive_x`
- `y` selects `positive_y`
- `Y` selects `negtive_y`
- `z` selects `positive_z`
- `Z` does not switch to `negtive_z`; that inward axis is fixed in this version
- `w` increases latitude
- `s` decreases latitude
- `a` decreases longitude
- `d` increases longitude
- `q` decreases roll
- `e` increases roll

## Context Menu

Right click in the viewport to toggle:

- `Rear hemisphere wireframe`
- `Local coordinate frame`
- `Y-axis great-circle trace`
- `X-axis tangential cylinder`
- `Global center (fix/move)`

### What The Toggles Do

`Rear hemisphere wireframe`

- Shows or hides wireframe lines on the far side of the sphere.

`Local coordinate frame`

- Shows or hides the local axis frame and component handles.
- When disabled, the anchor point remains visible as a white point.

`Y-axis great-circle trace`

- Shows or hides the great-circle trace associated with the local `y` axis.

`X-axis tangential cylinder`

- Shows or hides the tangential cylinder around the local frame.
- The cylinder radius follows `1.0 + positive_z_length`.
- The upper cylinder height follows `positive_y * (1.0 + positive_z_length)`.
- The lower cylinder height follows `negtive_y * (1.0 + positive_z_length)`.

`Global center (fix/move)`

- `fix`: the camera stays centered on the world origin.
- `move`: the camera centers on the current anchor point on the sphere.

## Behavior Notes

- Component lengths are clamped to a fixed range in code.
- The globe radius is `1.0`.
- Latitude is limited to `-89` to `89` degrees through keyboard input.
- Zoom is clamped to a fixed range through wheel input.
- `negtive_z` is fixed to length `1.0`, so its endpoint stays at the globe center and cannot be edited.
- The X-axis tangential cylinder uses a radius derived from `positive_z` and heights scaled by the positive/negative `y` component lengths.
- The application keeps the historical `negtive_*` spelling in labels and keys.

## Troubleshooting

### App starts but rendering is blank or broken

- Verify that Qt6 OpenGL-related packages are installed.
- Check that the machine can create an OpenGL context.
- If shader compilation fails because of a GLSL version mismatch, read [GLSL_COMPATIBILITY.md](/Users/cj/Documents/cdx-cj-nppm_dev_cpp_pixel_2_ray/GLSL_COMPATIBILITY.md).

### Binary does not exist

Rebuild the project:

```bash
cmake -S . -B build
cmake --build build
```

### Tests cannot be found

Make sure the project was configured with testing enabled. `include(CTest)` is already present in `CMakeLists.txt`, so standard CMake builds should generate the test targets.

## File Map

- [CMakeLists.txt](/Users/cj/Documents/cdx-cj-nppm_dev_cpp_pixel_2_ray/CMakeLists.txt): build targets and Qt dependencies
- [src/main.cpp](/Users/cj/Documents/cdx-cj-nppm_dev_cpp_pixel_2_ray/src/main.cpp): app startup and default surface format
- [src/MainWindow.cpp](/Users/cj/Documents/cdx-cj-nppm_dev_cpp_pixel_2_ray/src/MainWindow.cpp): main window layout and status panel wiring
- [src/PixelToRayWidget.cpp](/Users/cj/Documents/cdx-cj-nppm_dev_cpp_pixel_2_ray/src/PixelToRayWidget.cpp): rendering, input handling, and context menu behavior
- [src/FrameMath.cpp](/Users/cj/Documents/cdx-cj-nppm_dev_cpp_pixel_2_ray/src/FrameMath.cpp): coordinate and frame math
- [src/InteractionLogic.cpp](/Users/cj/Documents/cdx-cj-nppm_dev_cpp_pixel_2_ray/src/InteractionLogic.cpp): picking and drag projection logic
