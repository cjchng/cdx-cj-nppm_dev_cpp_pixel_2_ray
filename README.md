# Pixel To Ray Qt Rewrite

This directory contains a C++/Qt Widgets rewrite of the original browser-based sphere-frame demo.

## What It Includes

- `QMainWindow` shell with a live status panel
- `QOpenGLWidget` viewport for drawing the sphere, frame, and draggable component handles
- Ported local-frame math from the JavaScript demo
- Keyboard, mouse drag, wheel zoom, and right-click context menu controls

## Build

You need a C++17 compiler, CMake, and Qt6 Widgets development packages.

```bash
cmake -S . -B build
cmake --build build
./build/pixel_2_ray_qt
```

To run the C++ tests:

```bash
cd build
ctest --output-on-failure
```

## Controls

- Left click a handle to select and drag its length
- Right click to open the context menu
- Mouse wheel to zoom
- `x` / `X`, `y` / `Y`, `z` / `Z` to select signed axis components
- `w` / `s` to increase / decrease latitude
- `a` / `d` to decrease / increase longitude
- `q` / `e` to decrease / increase roll

## Notes

- `Global center (fix/move)` is implemented in this version: when set to `move`, the viewport centers on the current anchor point instead of the world origin.
- The original spelling `negtive_*` is preserved to stay aligned with the existing demo state names.
- The viewport now uses a shader-based OpenGL line/point renderer inside `QOpenGLWidget`.
