# Pixel To Ray Qt Rewrite

This repository contains a C++/Qt Widgets rewrite of the original browser-based sphere-frame demo.

## Current Features

- `QMainWindow` shell with a live comparison-style control panel
- `QOpenGLWidget` viewport for the globe, local frame, global traces, and cylinder
- keyboard, drag, wheel, and right-click controls
- globe texture mapping from a selectable fisheye image
- saved `current` and `default` state profiles in JSON
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

- `default_state.json`
- `current_state.json`

Repo-root symlinks point to the real files under:

`~/Library/Application Support/cj/pixel_to_ray_qt/state_profiles/`

## Notes

- `circular.png` is the built-in default fisheye image when no saved/default profile overrides it.
- `Global center (fix/move)` changes camera targeting, not the physical sphere center.
- Global traces now use a separate palette and dashed/terminal-marker styling so they read differently from the local frame.
- The historical spelling `negtive_*` is intentionally preserved in state keys and UI labels.
