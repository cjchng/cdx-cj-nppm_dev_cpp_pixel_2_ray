## X-Tangential Cylinder Note

### Purpose

The viewport can render an optional x-tangential cylinder attached to the current local frame on the globe.

This note describes how that cylinder is constructed in the current implementation.

### Entry Point

The cylinder is generated in:

- [src/PixelToRayWidget.cpp](/Users/cj/Documents/cdx-cj-nppm_dev_cpp_pixel_2_ray/src/PixelToRayWidget.cpp)
  - `appendPanoramaBand(...)`

The cylinder is only built when:

- `m_state.showXTangentCylinder == true`

### Frame Used

The cylinder is expressed in the current local frame:

- `frame.xAxis`
- `frame.yAxis`
- `frame.zAxis`

That frame already includes:

- local latitude / longitude / roll
- globe orientation rotation

So the cylinder follows the current local frame on the rotated globe.

### Radius

The cylinder radius is:

```cpp
xCylinderRadius = globeRadius + positiveZ
```

In code:

```cpp
const float xCylinderRadius =
    m_globeRadius + m_components[static_cast<int>(FrameMath::ComponentId::PositiveZ)];
```

### Heights

The current implementation uses multiplicative heights:

```cpp
positiveHeight = positiveY * xCylinderRadius
negativeHeight = negtiveY * xCylinderRadius
```

In code:

```cpp
const float positiveHeight = positiveY * xCylinderRadius;
const float negativeHeight = negativeY * xCylinderRadius;
```

### Circular Base

The cylinder base circle is parameterized in the local `z-x` plane:

```cpp
circlePoint(frame.zAxis, frame.xAxis, azimuth, xCylinderRadius)
```

This means the cylinder axis is aligned with:

- positive `y`
- negative `y`

So the wall extends upward and downward along `frame.yAxis`.

### Surface Construction

For each azimuth segment:

1. compute two neighboring base-circle points
2. extrude them upward by `+frame.yAxis * positiveHeight`
3. extrude them downward by `-frame.yAxis * negativeHeight`
4. emit two triangles for the upper wall quad
5. emit two triangles for the lower wall quad

Current wall pieces:

- upper wall:
  - `base0, base1, top1`
  - `base0, top1, top0`
- lower wall:
  - `bottom0, bottom1, base1`
  - `bottom0, base1, base0`

### Surface Coloring

The cylinder wall no longer uses translucent fill.

Current fill alpha:

```cpp
positiveFillAlpha = 1.0f
negativeFillAlpha = 1.0f
```

### Surface Texture Mapping

The cylinder wall is now textured indirectly from the globe fisheye image.

The method is:

1. take each wall triangle
2. compute its triangle centroid
3. interpret that centroid direction relative to the globe
4. inverse-map that direction into the fisheye image
5. sample the image with nearest-pixel lookup
6. use that sampled color as the triangle fill color

In code this is done with:

- `sampleProjectionColorNearest(...)`

If a valid fisheye sample is not available, the renderer falls back to the cylinder fill colors:

- `xPositiveCylinderFill`
- `xNegativeCylinderFill`

### Rear/Front Styling

The cylinder still distinguishes front-facing and rear-facing segments for overlay traces:

- front-facing trace alpha is stronger
- rear-facing trace alpha is weaker
- rear-facing segments may receive hatch-like diagonal lines

This affects the line overlay, not the opaque fill alpha.

### Great-Circle Companion Trace

When the x-tangential cylinder is visible, the code also renders a circular trace at the cylinder radius:

```cpp
appendGreatCircleTrace(frame.zAxis, frame.xAxis, xPositiveTraceColor, xCylinderRadius);
```

This companion trace lies in the same local `z-x` circular plane as the cylinder base.

### Rendering Pass

The cylinder surface is rendered in its own surface batch:

- `cylinderSurfaceVertices`

This was split from the globe surface so cylinder-specific depth-visibility behavior could be managed independently.

### Interaction

The cylinder is not edited directly by dragging its surface.

Its shape is derived from:

- globe radius
- `positive_z`
- `positive_y`
- `negtive_y`
- current local frame
- current globe orientation

### Related Files

- [src/PixelToRayWidget.cpp](/Users/cj/Documents/cdx-cj-nppm_dev_cpp_pixel_2_ray/src/PixelToRayWidget.cpp)
- [src/PixelToRayWidget.h](/Users/cj/Documents/cdx-cj-nppm_dev_cpp_pixel_2_ray/src/PixelToRayWidget.h)
- [USER_MANUAL.md](/Users/cj/Documents/cdx-cj-nppm_dev_cpp_pixel_2_ray/USER_MANUAL.md)
