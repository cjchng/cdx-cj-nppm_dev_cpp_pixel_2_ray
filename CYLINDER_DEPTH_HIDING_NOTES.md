## Cylinder Depth-Hiding Note

### Problem

The existing `Surface Depth Hiding` toggle was visually obvious on the globe, but not on the x-axis tangential cylinder.

### Why

1. The globe and cylinder were drawn together in one shared surface pass.
2. The cylinder already used translucent fills, dimmed rear segments, and hatch-like rear traces.
3. The single depth toggle only changed depth test/write for the whole shared surface batch.

That meant the cylinder already looked partially x-ray-like before the toggle changed anything, so the visual delta was weak.

### Refactor

The rendering is split into two surface passes:

1. `globeSurfaceVertices`
2. `cylinderSurfaceVertices`

This allows the cylinder to use a stronger no-depth presentation without changing the globe pass behavior.

### New Behavior

When `Surface Depth Hiding` is `on`:

- globe surface uses depth test/write
- cylinder surface uses depth test/write

When `Surface Depth Hiding` is `off`:

- globe surface renders without depth hiding
- cylinder surface renders in a stronger x-ray style:
  - no depth test/write
  - stronger rear fill visibility
  - stronger rear hatch/trace contrast

### Files

- `src/PixelToRayWidget.cpp`
- `src/PixelToRayWidget.h`
