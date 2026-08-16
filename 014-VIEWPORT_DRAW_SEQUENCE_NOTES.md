# Viewport Draw Sequence Notes

## Purpose

This note summarizes the viewport draw-sequence review for `PixelToRayWidget`, including the current rendering order, the main order/state risks, and a recommended rendering policy.

## Current Draw Sequence

The current viewport assembly and draw order is in [src/PixelToRayWidget.cpp](/Users/cj/Documents/cdx-cj-nppm_dev_cpp_pixel_2_ray/src/PixelToRayWidget.cpp:290).

Geometry is assembled in this order:

1. sphere surface
2. panorama band / cylinder geometry
3. sphere wireframe
4. frame geometry
5. active point highlight

It is then drawn in this order:

1. `surfaceVertices` as `GL_TRIANGLES`
2. `lineVertices` as `GL_TRIANGLES`
3. `activeLineVertices` as `GL_TRIANGLES`
4. `pointVertices` as `GL_POINTS`
5. `activePointVertices` as `GL_POINTS`

## Key Problems Identified

### 1. Triangle Passes Do Not Write Depth

In [drawVertices()](/Users/cj/Documents/cdx-cj-nppm_dev_cpp_pixel_2_ray/src/PixelToRayWidget.cpp:836), all `GL_TRIANGLES` passes disable depth writes:

```cpp
if (mode == GL_TRIANGLES) {
    glDepthMask(GL_FALSE);
}
```

That means:

- sphere surface triangles do not populate the depth buffer
- cylinder wall triangles do not populate the depth buffer
- thick line quads do not populate the depth buffer

As a result, later triangle passes can visually overpaint earlier geometry even when they should be hidden behind it.

### 2. Transparent Geometry Is Not Ordered As Transparent Geometry

Blending is enabled globally in [initializeGL()](/Users/cj/Documents/cdx-cj-nppm_dev_cpp_pixel_2_ray/src/PixelToRayWidget.cpp:267).

However:

- translucent sphere surface triangles
- translucent cylinder wall triangles
- translucent thick-line quads

are drawn in a fixed append order, not in a back-to-front sorted order.

This is not a reliable blending policy for overlapping translucent geometry.

### 3. Frame-Hidden Mode Still Leaves The Active Point

When `showLocalFrame` is off, [appendFrameGeometry()](/Users/cj/Documents/cdx-cj-nppm_dev_cpp_pixel_2_ray/src/PixelToRayWidget.cpp:793) suppresses frame geometry and leaves only the anchor point.

But [paintGL()](/Users/cj/Documents/cdx-cj-nppm_dev_cpp_pixel_2_ray/src/PixelToRayWidget.cpp:312) still computes and draws the active point highlight unconditionally.

That means the viewport can still show an “active component tip” even when the local frame is supposed to be hidden.

### 4. Thick-Line Orientation Depends On Global-Center Camera Target

The thick-line quad generator in [appendLine()](/Users/cj/Documents/cdx-cj-nppm_dev_cpp_pixel_2_ray/src/PixelToRayWidget.cpp:594) builds its camera-facing offset from:

```cpp
QVector3D viewForward = (currentSceneCenter() - cameraPosition()).normalized();
```

Because `currentSceneCenter()` is also tied to the global-center toggle, line-quad orientation depends on the current camera-target policy instead of only on stable camera/view geometry.

### 5. Full Geometry Rebuild Every Frame

Every repaint:

- allocates fresh vectors
- regenerates all sphere, cylinder, wireframe, frame, and point geometry
- uploads each pass separately with `m_vertexBuffer.allocate(...)`

That is acceptable for a small interactive demo, but it makes the draw sequence the natural performance hotspot if the scene becomes more complex.

## Recommended Render Policy

The viewport should choose one clear model:

### Option A: Correct Depth / Transparency

Recommended order:

1. opaque surfaces first, with depth writes enabled
2. translucent surfaces next, ideally back-to-front
3. guide overlays after that, with explicit intent
4. points and active highlight last

This gives the cleanest physical rendering behavior.

### Option B: Always-Visible Guide Overlay

If the design goal is that guides should always remain visible on top of surfaces, then that should be explicit:

1. surfaces first
2. overlay guides with a deliberate “always on top” policy
3. active points last

This is less physically correct, but easier to control visually.

## Recommended Sequence For This Repo

For this project, the most maintainable approach is:

1. draw opaque sphere/cylinder surfaces first with depth writes enabled
2. draw translucent surfaces next, sorted if necessary
3. draw wireframe and frame guides after surfaces, using a deliberate overlay rule
4. draw points and active highlight last

That would make the viewport behavior easier to reason about and reduce current order-dependent artifacts.

## Related Files

- [src/PixelToRayWidget.cpp](/Users/cj/Documents/cdx-cj-nppm_dev_cpp_pixel_2_ray/src/PixelToRayWidget.cpp): viewport assembly and rendering
- [src/PixelToRayWidget.h](/Users/cj/Documents/cdx-cj-nppm_dev_cpp_pixel_2_ray/src/PixelToRayWidget.h): render-path declarations
