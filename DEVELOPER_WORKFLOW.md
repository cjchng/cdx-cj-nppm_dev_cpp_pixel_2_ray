# Developer Workflow

This file describes the normal edit-build-test loop for working on this project after the machine has already been configured.

If setup is still failing, start with [SETUP_AND_TROUBLESHOOTING.md](/home/cj/Documents/cdx-cj-nppm_dev_cpp_pixel_2_ray/SETUP_AND_TROUBLESHOOTING.md).

## Normal Daily Loop

From the project root:

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
./build/pixel_2_ray_qt
```

Use this loop when you change C++ code, UI behavior, rendering, or tests.

## What To Run After Different Kinds Of Changes

If you changed only implementation code in `src/`:

```bash
cmake --build build
ctest --test-dir build --output-on-failure
```

If you changed `CMakeLists.txt`, added source files, or changed dependencies:

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

If you changed only tests:

```bash
cmake --build build
ctest --test-dir build --output-on-failure
```

If the build directory seems stale or inconsistent:

```bash
rm -rf build
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

## Main Targets In This Project

Configured in [CMakeLists.txt](/home/cj/Documents/cdx-cj-nppm_dev_cpp_pixel_2_ray/CMakeLists.txt):

- `pixel_2_ray_core`: shared library with math and interaction logic
- `pixel_2_ray_qt`: desktop Qt application
- `pixel_2_ray_frame_tests`: frame math tests
- `pixel_2_ray_interaction_tests`: interaction logic tests

## Where To Read Before Editing

Suggested reading order:

1. [CMakeLists.txt](/home/cj/Documents/cdx-cj-nppm_dev_cpp_pixel_2_ray/CMakeLists.txt)
2. [src/main.cpp](/home/cj/Documents/cdx-cj-nppm_dev_cpp_pixel_2_ray/src/main.cpp)
3. [src/MainWindow.h](/home/cj/Documents/cdx-cj-nppm_dev_cpp_pixel_2_ray/src/MainWindow.h)
4. [src/MainWindow.cpp](/home/cj/Documents/cdx-cj-nppm_dev_cpp_pixel_2_ray/src/MainWindow.cpp)
5. [src/PixelToRayWidget.h](/home/cj/Documents/cdx-cj-nppm_dev_cpp_pixel_2_ray/src/PixelToRayWidget.h)
6. [src/PixelToRayWidget.cpp](/home/cj/Documents/cdx-cj-nppm_dev_cpp_pixel_2_ray/src/PixelToRayWidget.cpp)
7. [src/FrameMath.h](/home/cj/Documents/cdx-cj-nppm_dev_cpp_pixel_2_ray/src/FrameMath.h)
8. [src/FrameMath.cpp](/home/cj/Documents/cdx-cj-nppm_dev_cpp_pixel_2_ray/src/FrameMath.cpp)
9. [src/InteractionLogic.h](/home/cj/Documents/cdx-cj-nppm_dev_cpp_pixel_2_ray/src/InteractionLogic.h)
10. [src/InteractionLogic.cpp](/home/cj/Documents/cdx-cj-nppm_dev_cpp_pixel_2_ray/src/InteractionLogic.cpp)
11. [tests/FrameMathTests.cpp](/home/cj/Documents/cdx-cj-nppm_dev_cpp_pixel_2_ray/tests/FrameMathTests.cpp)
12. [tests/InteractionLogicTests.cpp](/home/cj/Documents/cdx-cj-nppm_dev_cpp_pixel_2_ray/tests/InteractionLogicTests.cpp)

## Practical Development Advice

- Keep logic changes in `pixel_2_ray_core` when possible so they stay testable outside the widget.
- Add or update a test when you change frame math or interaction state rules.
- Re-run `cmake -S . -B build` after changing target names, adding files, or changing required packages.
- Read the first real CMake failure, not the long cascade of dependent package failures after it.
- For rendering bugs, separate "math state is wrong" from "drawing is wrong" before editing.

## Common Commands

Run the app:

```bash
./build/pixel_2_ray_qt
```

Run all tests:

```bash
ctest --test-dir build --output-on-failure
```

Build only:

```bash
cmake --build build
```

Reconfigure from scratch:

```bash
rm -rf build
cmake -S . -B build
```

## When You Are Exploring A New Feature

A safe workflow is:

1. Read the relevant source and test files first.
2. Make the smallest code change that proves the idea.
3. Rebuild immediately.
4. Run tests immediately.
5. Launch the app and verify the behavior manually.
6. Only then continue refining the implementation.
