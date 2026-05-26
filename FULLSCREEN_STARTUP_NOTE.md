## Fullscreen Startup Note

### Symptom

On macOS, launching the app could print:

```text
not in fullscreen state
not in fullscreen state
```

### Actual Causes Found

The issue turned out to be a sequence problem, not one single bug.

1. The app originally mixed normal-window positioning with immediate fullscreen entry.
2. `MainWindow` entered fullscreen internally, but `main.cpp` also called `window.show()`.
3. Fullscreen entry was happening too early in startup, directly from the constructor.

### Fix Sequence

The startup path was revised in three steps:

1. remove normal-window `move(...)` from the fullscreen path
2. remove the extra `window.show()` call in `main.cpp`
3. defer fullscreen entry with `QTimer::singleShot(0, ...)`

The current startup flow is:

1. create the main window
2. let the event loop start
3. resolve the preferred screen
4. bind the window handle to that screen
5. call `showFullScreen()`

### Code

- [src/MainWindow.cpp](/Users/cj/Documents/cdx-cj-nppm_dev_cpp_pixel_2_ray/src/MainWindow.cpp)
  - `preferredStartupScreen(...)`
  - `showWindowFullScreenOnPreferredScreen(...)`
  - deferred fullscreen call from the constructor
- [src/main.cpp](/Users/cj/Documents/cdx-cj-nppm_dev_cpp_pixel_2_ray/src/main.cpp)
  - no extra `window.show()`

### Current Status

The code now avoids the known conflicting startup patterns.

If the warning still appears on a specific macOS/Qt combination, the next fallback would be:

- start non-fullscreen
- provide a user-triggered fullscreen action after launch
