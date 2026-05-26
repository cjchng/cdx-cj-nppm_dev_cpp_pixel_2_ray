# Pixel To Ray User Manual

## Overview

`pixel_2_ray_qt` is a Qt6 desktop application for visualizing:

- a globe and its mapped fisheye image
- a local coordinate frame attached to the globe surface
- editable signed local component lengths
- globe-fixed global traces and terminal markers
- an optional x-axis tangential cylinder

The application is an interactive viewer and teaching tool. Most values can be changed from the right panel, and several viewport operations are available directly by mouse or keyboard.

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

- Left: OpenGL viewport
- Right: comparison-style control panel with `Current` and `Default` columns

The `Current` and `Default` column headers are clickable. They switch the live runtime state between the saved profile and the default profile.

## Right Panel

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
  - globe quaternion `q_w / q_x / q_y / q_z`
- `Camera Pose`
  - perspective `FOV Y`
  - eye offsets
  - target `X/Y/Z`
  - up vector
- `Components`
  - `positive_x`
  - `negtive_x`
  - `positive_y`
  - `negtive_y`
  - `positive_z`
  - fixed `negtive_z`

Rows with `Lock` checkboxes block both panel edits and matching interactive edits in the viewport.

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
- `ESC` quits

## Commands Menu

The menu bar includes `Commands`:

- `Set Fisheye Image...`
- `Reload Fisheye Image`
- `Clear Fisheye Image`

The selected fisheye file path is shown in the right panel as a basename, with the full path in the tooltip.

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
- Cylinder surface texturing samples the globe image by inverse mapping and uses nearest-pixel lookup.

## Geometry Notes

- The default globe radius is `1.0`.
- `negtive_z` is fixed to the globe radius and is not editable.
- The x-axis tangential cylinder radius is:

`globe_radius + positive_z`

- Cylinder heights are:

`positive_y * xCylinderRadius`

`negtive_y * xCylinderRadius`

## Global vs Local Visual Language

- Local axes remain the main saturated frame colors.
- Global traces use a separate high-contrast palette.
- Global spokes are thinner and dashed.
- Global positive endpoints use diamond terminal markers.
- Global negative endpoints use `X` terminal markers.
- The local origin uses its own marker shape and does not match endpoint markers.

## Saved Profiles

The application now stores two JSON profiles:

- `default_state.json`
- `current_state.json`

The real files are stored under:

`~/Library/Application Support/cj/pixel_to_ray_qt/state_profiles/`

Repo-root symlinks are provided:

- [default_state.json](/Users/cj/Documents/cdx-cj-nppm_dev_cpp_pixel_2_ray/default_state.json)
- [current_state.json](/Users/cj/Documents/cdx-cj-nppm_dev_cpp_pixel_2_ray/current_state.json)

Behavior:

- editing while `Current` is active updates `current_state.json`
- editing while `Default` is active updates `default_state.json`
- the built-in fallback default fisheye path is `circular.png`

## Fullscreen Startup

The app starts fullscreen on the preferred screen. The startup path was adjusted to reduce macOS fullscreen warnings by:

- avoiding normal-window `move(...)` before fullscreen
- removing the extra `window.show()` path
- deferring fullscreen entry until the event loop starts

See [FULLSCREEN_STARTUP_NOTE.md](/Users/cj/Documents/cdx-cj-nppm_dev_cpp_pixel_2_ray/FULLSCREEN_STARTUP_NOTE.md).

## File Map

- [README.md](/Users/cj/Documents/cdx-cj-nppm_dev_cpp_pixel_2_ray/README.md): quick repo overview
- [src/main.cpp](/Users/cj/Documents/cdx-cj-nppm_dev_cpp_pixel_2_ray/src/main.cpp): app bootstrap
- [src/MainWindow.cpp](/Users/cj/Documents/cdx-cj-nppm_dev_cpp_pixel_2_ray/src/MainWindow.cpp): window, panel, commands menu, JSON profiles
- [src/PixelToRayWidget.cpp](/Users/cj/Documents/cdx-cj-nppm_dev_cpp_pixel_2_ray/src/PixelToRayWidget.cpp): rendering and viewport interaction
- [src/FrameMath.cpp](/Users/cj/Documents/cdx-cj-nppm_dev_cpp_pixel_2_ray/src/FrameMath.cpp): coordinate and frame math
