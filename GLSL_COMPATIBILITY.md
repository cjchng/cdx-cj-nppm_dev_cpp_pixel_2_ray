# GLSL Compatibility Note

## Symptom

On some machines, launching `./build/pixel_2_ray_qt` failed with a shader compile error similar to:

```text
QOpenGLShader::compile(Fragment): ERROR: 0:1: '' :  version '330' is not supported
ERROR: 0:1: '' : syntax error: #version
```

## Cause

The widget renderer originally hard-coded GLSL `#version 330 core` for both vertex and fragment shaders in `src/PixelToRayWidget.cpp`.

That works only when Qt creates a desktop OpenGL 3.3+ context. On systems that provide an older desktop OpenGL context, or a different profile than expected, the shader compiler rejects that version string.

## Resolution

The shader setup now selects shader source code based on the active `QOpenGLContext`:

- OpenGL 3.3+: `#version 330 core`
- OpenGL 3.2: `#version 150`
- Older desktop OpenGL: `#version 120`
- OpenGL ES: `#version 300 es`

Attribute bindings are applied explicitly so the same vertex layout works across these variants.

## Files Changed

- `src/PixelToRayWidget.cpp`

## Verification

After the change:

```bash
cmake --build build
cd build
ctest --output-on-failure
```

The project rebuilt successfully and all tests passed.
