# Setup And Troubleshooting

This note records the setup problems encountered while configuring this project on Ubuntu 22.04 and explains what each required build tool or library does.

Use it as the practical companion to [README.md](/home/cj/Documents/cdx-cj-nppm_dev_cpp_pixel_2_ray/README.md) when you need to rebuild the project or extend it.

## Required Packages

Install these packages on Ubuntu:

```bash
sudo apt update
sudo apt install -y \
  cmake \
  g++ \
  pkg-config \
  qt6-base-dev \
  qt6-base-dev-tools \
  libqt6opengl6-dev \
  libgl-dev \
  libopengl-dev
```

## What Each Tool Or Library Does

`cmake`

- Reads `CMakeLists.txt`.
- Detects compilers, Qt, OpenGL, and test dependencies.
- Generates the build directory and native build files.
- Provides `ctest`, which runs the tests declared in `include(CTest)`.

`g++`

- Compiles the C++17 source files in `src/` and `tests/`.
- Links the final executable and test binaries.

`pkg-config`

- General dependency discovery tool used by many Linux development packages.
- Not directly called by this project, but useful when CMake or Qt dependency detection needs system package metadata.

`qt6-base-dev`

- Main Qt6 development package for desktop apps.
- Provides headers, libraries, and CMake package files for core modules used here, especially `Qt6::Widgets`, `Qt6::Gui`, and `Qt6::Test`.
- This project needs it because `CMakeLists.txt` calls:

```cmake
find_package(Qt6 REQUIRED COMPONENTS Widgets Gui Test OpenGL OpenGLWidgets)
```

`qt6-base-dev-tools`

- Qt support tools used during build generation.
- Provides tools such as `moc`, `uic`, and related helpers needed by CMake features like `CMAKE_AUTOMOC`, `CMAKE_AUTOUIC`, and `CMAKE_AUTORCC`.

`libqt6opengl6-dev`

- Qt6 development package for OpenGL integration.
- Provides the CMake package files and link targets for `Qt6::OpenGL` and `Qt6::OpenGLWidgets`.
- This project needs it because the viewport uses `QOpenGLWidget`.

`libgl-dev`

- Provides standard OpenGL development headers, including `GL/gl.h`.
- Required so CMake can satisfy Qt's `WrapOpenGL` dependency.

`libopengl-dev`

- Provides the OpenGL vendor-neutral development files that CMake's `FindOpenGL` module expects on Ubuntu.
- Without it, CMake may report `Could NOT find OpenGL (missing: OPENGL_INCLUDE_DIR)`.

## Build Flow

From the project root:

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
./build/pixel_2_ray_qt
```

What each step does:

- `cmake -S . -B build` configures the project and checks dependencies.
- `cmake --build build` compiles the app and tests.
- `ctest --test-dir build --output-on-failure` runs the test executables created by CMake.
- `./build/pixel_2_ray_qt` launches the desktop application.

## Problems We Hit And How They Were Solved

### 1. `cmake` Was Not Installed

Observed error:

```text
Command 'cmake' not found
```

Session note:

```text
$ cmake -S . -B build
Command 'cmake' not found, but can be installed with:
snap install cmake
apt install cmake
```

Fix:

- Install Ubuntu's `cmake` package.
- Do not rely on a Snap-installed `cmake` for this project if you can avoid it.

Why:

- The project only requires `cmake >= 3.16`.
- Ubuntu 22.04's packaged `cmake 3.22.1` is sufficient.

### 2. Mixed `snap` And `apt` CMake Caused Confusing Qt Detection

Observed symptom:

- CMake found `/usr/lib/x86_64-linux-gnu/cmake/Qt6/Qt6Config.cmake`.
- But it still reported `Qt6_FOUND` as false and failed to resolve components like `Widgets`, `Gui`, `OpenGL`, and `OpenGLWidgets`.

Session note:

```text
Found package configuration file:
  /usr/lib/x86_64-linux-gnu/cmake/Qt6/Qt6Config.cmake
but it set Qt6_FOUND to FALSE
Failed to find Qt component "Widgets"
Failed to find Qt component "Gui"
Failed to find Qt component "OpenGL"
Failed to find Qt component "OpenGLWidgets"
```

Fix:

- Remove the Snap `cmake`.
- Install and use `/usr/bin/cmake` from Ubuntu `apt`.

Verification:

```bash
which cmake
cmake --version
```

Expected result:

- `which cmake` should print `/usr/bin/cmake`.

Observed good state from the session:

```text
/usr/bin/cmake
cmake version 3.22.1
```

### 3. Qt6 Was Installed, But System OpenGL Development Headers Were Missing

Observed error:

```text
Could NOT find OpenGL (missing: OPENGL_INCLUDE_DIR)
Could NOT find WrapOpenGL (missing: WrapOpenGL_FOUND)
Qt6Gui could not be found because dependency WrapOpenGL could not be found.
```

Fuller session excerpt:

```text
-- Could NOT find OpenGL (missing: OPENGL_INCLUDE_DIR)
-- Could NOT find WrapOpenGL (missing: WrapOpenGL_FOUND)
Qt6Gui could not be found because dependency WrapOpenGL could not be found.
Qt6Widgets could not be found because dependency Qt6Gui could not be found.
Qt6OpenGL could not be found because dependency Qt6Gui could not be found.
Qt6OpenGLWidgets could not be found because dependency Qt6OpenGL could not be found.
```

Fix:

```bash
sudo apt install -y libgl-dev libopengl-dev
```

Why:

- Qt's GUI and OpenGL modules depend on system OpenGL development headers.
- The Qt package config files were present, but they could not complete dependency resolution until the OpenGL headers were installed.

### 4. Qt Package Files Existing Does Not Guarantee A Working Build

Important lesson:

- Seeing files like `/usr/lib/x86_64-linux-gnu/cmake/Qt6Widgets/Qt6WidgetsConfig.cmake` is not enough.
- Those config files can still fail internally if their own dependencies are missing.

Practical takeaway:

- Always read the first missing dependency in the CMake output.
- In this setup, the real blocker was `WrapOpenGL`, not the absence of Qt package directories.

## Session Transcript Highlights

These were the key facts confirmed during the setup/debug sequence.

Initial configure failed because `cmake` was missing:

```text
$ cmake -S . -B build
Command 'cmake' not found
```

After installing packages, the machine had the expected package set:

```text
$ which cmake
/usr/bin/cmake

$ cmake --version
cmake version 3.22.1

$ dpkg -l | grep -E 'cmake|qt6-base|libqt6opengl'
ii  cmake
ii  cmake-data
ii  libqt6opengl6
ii  libqt6opengl6-dev
ii  libqt6openglwidgets6
ii  qt6-base-dev
ii  qt6-base-dev-tools
```

The Qt6 CMake package directories were present:

```text
/usr/lib/x86_64-linux-gnu/cmake/Qt6
/usr/lib/x86_64-linux-gnu/cmake/Qt6Widgets
/usr/lib/x86_64-linux-gnu/cmake/Qt6Gui
/usr/lib/x86_64-linux-gnu/cmake/Qt6OpenGL
/usr/lib/x86_64-linux-gnu/cmake/Qt6OpenGLWidgets
```

The actual remaining issue was OpenGL development headers, not missing Qt package files:

```text
Could NOT find OpenGL (missing: OPENGL_INCLUDE_DIR)
Could NOT find WrapOpenGL (missing: WrapOpenGL_FOUND)
```

Installing these packages resolved the final configure blocker:

```bash
sudo apt install -y libgl-dev libopengl-dev
```

Final outcome:

- CMake configured successfully.
- The user confirmed the build then worked.

## Fast Diagnostics

These commands are useful when setup breaks again:

```bash
which cmake
cmake --version
dpkg -l | grep -E 'cmake|qt6-base|libqt6opengl|libgl-dev|libopengl-dev'
ls /usr/lib/x86_64-linux-gnu/cmake/Qt6
ls /usr/lib/x86_64-linux-gnu/cmake/Qt6Widgets
ls /usr/lib/x86_64-linux-gnu/cmake/Qt6Gui
ls /usr/lib/x86_64-linux-gnu/cmake/Qt6OpenGL
ls /usr/lib/x86_64-linux-gnu/cmake/Qt6OpenGLWidgets
ls /usr/include/GL/gl.h
```

## How This Maps To The Codebase

`CMakeLists.txt`

- Defines the whole build.
- Requests the Qt modules and test support.

`src/main.cpp`

- Starts the Qt application.

`src/MainWindow.cpp`

- Builds the main desktop window.

`src/PixelToRayWidget.cpp`

- Contains the OpenGL widget and rendering behavior.
- This file is why Qt OpenGL support and system OpenGL headers are needed.

`tests/FrameMathTests.cpp`
`tests/InteractionLogicTests.cpp`

- Unit tests executed through `ctest`.

## Suggested Learning Path

If you want to start developing features safely, this is a good order:

1. Read `CMakeLists.txt` to understand targets, dependencies, and tests.
2. Read `src/main.cpp` and `src/MainWindow.cpp` to understand the application shell.
3. Read `src/PixelToRayWidget.h` and `src/PixelToRayWidget.cpp` to understand rendering and interaction.
4. Read `src/FrameMath.*` and `src/InteractionLogic.*` to understand the math and non-UI logic.
5. Run `ctest --test-dir build --output-on-failure` after each meaningful change.

## Known-Good Rebuild Command Set

When the environment is already installed correctly, this is the normal rebuild loop:

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
./build/pixel_2_ray_qt
```
