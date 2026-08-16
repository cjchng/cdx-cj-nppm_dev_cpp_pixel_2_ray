# Cylinder Shader Texture Plan

## Purpose

This note refactors the plan for the `x`-axis tangential cylinder texture path.

The goal is to replace the current triangle-centroid color approximation with a GPU shader path that projects every visible cylinder fragment back onto the globe image.

The refactor should keep the current UI and geometry behavior:

- the cylinder is still attached to the current local frame
- the cylinder radius still comes from `globeRadius + positive_z`
- the positive and negative cylinder heights still come from `positive_y` and `negtive_y`
- surface-depth hiding and guide overlays should keep their current behavior
- the existing CPU fallback colors should remain available when no valid image sample exists

## Current Implementation

The current cylinder surface is generated in:

- `src/PixelToRayWidget.cpp`
- `PixelToRayWidget::appendPanoramaBand(...)`

The active texture sampling path is now:

1. Build the x-tangential cylinder wall triangles in world coordinates.
2. Emit matching `TexturedCylinderVertex` triangles with fallback colors.
3. Draw them with `drawTexturedCylinderVertices(...)`.
4. In the fragment shader, normalize the interpolated world position as the direction from the global center.
5. Rotate that direction into globe-local coordinates.
6. Convert the globe-local direction into fisheye image texture coordinates.
7. Sample the uploaded fisheye `QOpenGLTexture` with nearest filtering.
8. Fall back to `xPositiveCylinderFill` or `xNegativeCylinderFill` if the shader cannot use the image sample.

The legacy CPU comparison path is still generated:

1. Build each azimuth segment as four triangles.
2. Compute one centroid per wall triangle.
3. Call `sampleProjectionColorNearest(...)` for that centroid.
4. Fill the whole triangle with that one sampled color.
5. Fall back to `xPositiveCylinderFill` or `xNegativeCylinderFill` if the sample is invalid.

That legacy path created visible flat patches because the cylinder wall was not sampled continuously. It remains useful for fallback/comparison work until the shader path has been visually accepted.

The relevant current calls are:

```cpp
const QColor positiveSampleA = sampleProjectionColorNearest((base0 + base1 + top1) / 3.0f);
const QColor positiveSampleB = sampleProjectionColorNearest((base0 + top1 + top0) / 3.0f);
const QColor negativeSampleA = sampleProjectionColorNearest((bottom0 + bottom1 + base1) / 3.0f);
const QColor negativeSampleB = sampleProjectionColorNearest((bottom0 + base1 + base0) / 3.0f);
```

The shared color-only `Vertex` type still carries:

```cpp
QVector3D position;
QVector4D color;
```

So the shared color-only shader can only draw pre-colored geometry. The cylinder shader uses `TexturedCylinderVertex` instead.

## Target Behavior

The target cylinder is textured by per-fragment projection:

1. The CPU emits a cylinder wall mesh.
2. The vertex shader passes each fragment enough world-space information to identify the wall point.
3. The fragment shader projects that wall point back toward the globe center.
4. The projected globe direction is converted into fisheye image coordinates.
5. The shader samples the fisheye texture.

The important change is where the lookup happens:

- legacy CPU path: one CPU image lookup per triangle
- active shader path: one GPU texture lookup per fragment

That removes the coarse two-sample-per-side look along the `y` direction.

## Coordinate Contract

The cylinder is expressed in the current local frame:

```text
frame.xAxis
frame.yAxis
frame.zAxis
```

That frame already includes the global globe orientation.

Use these state-derived values:

```text
R    = globeRadius + positive_z
Hpos = positive_y * R
Hneg = negtive_y * R
```

Parameterize the cylinder wall with:

```text
u in [0, 1]  azimuth around the cylinder
v in [0, 1]  height from negative y to positive y
```

Then:

```text
phi = 2*pi*u
h   = mix(-Hneg, Hpos, v)

base(u) = R*cos(phi)*frame.zAxis + R*sin(phi)*frame.xAxis
P(u,v)  = base(u) + h*frame.yAxis
```

`P(u,v)` is the world-space cylinder wall point.

The "global center" in this projection means the geometric globe center at world `(0, 0, 0)`. This is the same center used by the sphere and by the existing texture lookup. It is not the camera target selected by the `Fix Global Center` view toggle.

## Projection Contract

For a cylinder wall point `P`, compute the globe direction:

```text
worldDir = normalize(P)
```

The globe intersection is conceptually:

```text
G = worldDir * globeRadius
```

The shader does not need `G` unless future logic needs the physical globe hit position. The fisheye lookup only needs the normalized direction.

Convert the world direction into globe-local coordinates:

```text
globeLocal = inverseGlobalOrientation * worldDir
```

In the current CPU code this is equivalent to:

```cpp
const QVector3D globeLocal =
    m_globeOrientation.conjugated().rotatedVector(worldPoint).normalized();
```

## Fisheye Mapping Contract

The shader should match the current CPU mapping used by `sampleProjectionColor(...)` and `sampleProjectionColorNearest(...)`.

Given `globeLocal`:

```text
theta = acos(clamp(globeLocal.z, -1, 1))
```

Reject or fall back when:

```text
theta > capAngleRadians
```

The current cap angle is:

```text
140 degrees
```

The current radial normalization is:

```text
radiusNormalized = sin(theta / 2) / sin(capAngleRadians / 2)
```

The current image direction is:

```text
xyLength = length(globeLocal.xy)

if xyLength is near zero:
    dir = (0, 0)
else:
    dir = globeLocal.xy / xyLength
```

Convert to image pixel coordinates:

```text
pixelX = circleCenter.x + radiusNormalized * circleRadius * dir.x
pixelY = circleCenter.y - radiusNormalized * circleRadius * dir.y
```

Then normalize to texture coordinates:

```text
texU = pixelX / imageWidth
texV = pixelY / imageHeight
```

The texture upload must account for Qt image row order. The CPU path uses top-left image pixel coordinates, while OpenGL texture sampling commonly treats `v = 0` as the bottom row unless the upload or shader flips the coordinate.

## Shader Split

### Practical First Step

Keep the CPU responsible for cylinder mesh generation. Add a dedicated textured-cylinder vertex type that includes world position and fallback color:

```cpp
struct TexturedCylinderVertex {
    QVector3D position;
    QVector4D fallbackColor;
};
```

The vertex shader should:

- transform `position` by `uMvp`
- pass `position` to the fragment shader as world position
- pass `fallbackColor` to the fragment shader

The fragment shader should:

- compute `worldDir = normalize(vWorldPosition)`
- compute `globeLocal`
- compute fisheye texture coordinates
- sample the fisheye texture when valid
- return `fallbackColor` when invalid or when no texture is available

This is the lowest-risk migration because the CPU geometry math can stay close to the current implementation.

### Later Analytic Variant

A later version can pass only `(u, v)` plus cylinder parameters and reconstruct `P(u,v)` in the vertex shader.

That is a cleaner geometry contract, but it requires more uniform plumbing. It should come after the first shader path is working.

## Required Uniforms

The textured-cylinder shader will need:

```text
uMvp
uUseTexture
uFisheyeTexture
uInverseGlobeOrientation
uCapAngleRadians
uProjectionCircleCenterPx
uProjectionCircleRadiusPx
uProjectionImageSizePx
```

Optional uniforms for future analytic reconstruction:

```text
uGlobeRadius
uCylinderRadius
uPositiveHeight
uNegativeHeight
uFrameXAxis
uFrameYAxis
uFrameZAxis
uGlobalCenter
```

For the first implementation, `uGlobalCenter` can remain implicit as `(0, 0, 0)` because that matches the current globe geometry and CPU sampler.

## Texture Object Requirements

The current renderer keeps the fisheye source as a `QImage` for CPU sampling. The shader path also needs a GPU texture object.

Implementation requirements:

- upload the loaded projection image into a `QOpenGLTexture`
- refresh the texture whenever `loadProjectionImage(...)` loads a new image
- keep the CPU `QImage` path during migration for the globe and for comparison
- decide whether the first shader pass should use nearest filtering or linear filtering

To match the current cylinder behavior and the original nearest-pixel requirement, start with nearest filtering:

```text
minification filter = nearest
magnification filter = nearest
```

Linear filtering can be considered later if the visual goal changes from exact nearest-pixel lookup to smoother texture interpolation.

## Mesh Resolution

The current azimuth segmentation is:

```text
kPanoramaSegments = 144
```

That can remain the first shader implementation value.

Because fragment sampling supplies texture detail, the mesh no longer needs to carry color detail. Mesh density is only for geometry shape. A practical starting point is:

```text
azimuth segments = 144
height segments  = 2, 8, or 16
```

Height subdivision is not as important as azimuth subdivision because the cylinder is straight along `frame.yAxis`. More height bands may still help if the shader initially receives interpolated world positions from planar triangles and the visual result needs finer geometric approximation.

## CPU Refactor Steps

1. Add a textured-cylinder shader program separate from the existing color-only program. Done as inactive scaffolding.
2. Add a textured-cylinder vertex type and buffer setup. Done as inactive scaffolding.
3. Upload the fisheye `QImage` into a `QOpenGLTexture`. Done as inactive texture upload plumbing.
4. Split cylinder surface generation from `appendPanoramaBand(...)` into a dedicated mesh builder. Done as `appendXTangentCylinder(...)`.
5. Keep the existing line traces and hatch overlays in the current color-only line path. Done.
6. Draw the textured cylinder surface in the surface pass before guide overlays. Done with `kUseTexturedCylinderShader = true`.
7. Remove triangle-centroid calls to `sampleProjectionColorNearest(...)` from the inactive CPU comparison path once the shader output is visually accepted.

The CPU fallback colors should remain:

```text
xPositiveCylinderFill
xNegativeCylinderFill
```

Use them for:

- no projection image
- invalid cap sample
- invalid projection circle
- temporary shader-disabled debug mode

## Incremental Refactor Log

### Step 1: Extract Current Cylinder Helper

Status: done.

Files:

- `src/PixelToRayWidget.h`
- `src/PixelToRayWidget.cpp`

Changes:

- Added `appendXTangentCylinder(...)`.
- Moved the existing x-tangential cylinder surface triangles, nearest-pixel centroid samples, front/rear trace alpha, and hatch lines into that helper.
- Kept `appendPanoramaBand(...)` as the higher-level coordinator for the x-cylinder and y-axis panorama trace.
- Added shared `frameCirclePoint(...)` math for local-frame circular points.

Behavior should be unchanged after this step. The cylinder still uses CPU triangle-centroid sampling. This step only separates the current implementation so the shader path can replace the surface drawing in a later step.

Next step:

- Add a textured-cylinder vertex type and shader-program scaffolding without changing the visible output yet.

### Step 2: Add Inactive Textured-Cylinder GL Scaffolding

Status: done.

Files:

- `src/PixelToRayWidget.h`
- `src/PixelToRayWidget.cpp`

Changes:

- Added `TexturedCylinderVertex` with world `position` and `fallbackColor`.
- Added a dedicated textured-cylinder shader program, VAO, and VBO.
- The textured-cylinder vertex shader passes world position and fallback color to the fragment shader.
- The textured-cylinder fragment shader contains the planned per-fragment fisheye projection math and fallback-color path.
- The current cylinder draw path still uses the existing CPU-colored `Vertex` path; no visible rendering switch happens in this step.

Next step:

- Upload the fisheye `QImage` into a `QOpenGLTexture` and refresh it when `loadProjectionImage(...)` changes the source image.

### Step 3: Upload Projection Image As GL Texture

Status: done.

Files:

- `src/PixelToRayWidget.h`
- `src/PixelToRayWidget.cpp`

Changes:

- Added `m_projectionTexture` as a `QOpenGLTexture`.
- Added `m_projectionTextureDirty` so image load/clear can be requested before an OpenGL context is current.
- Added `refreshProjectionTexture()` and call it from `ensureGlResources()`.
- Marked the texture dirty when `loadProjectionImage(...)` replaces the image.
- Marked the texture dirty when `setFisheyeImagePath(QString())` clears the image.
- Uploaded the current `QImage` with nearest filtering and clamp-to-edge wrap mode.

The texture is now available for the future textured-cylinder draw path, but the app still renders the cylinder through the current CPU-colored path.

Next step:

- Split cylinder surface mesh creation into textured-cylinder vertices while still drawing the current CPU-colored cylinder for visual parity.

### Step 4: Generate Textured-Cylinder Mesh In Parallel

Status: done.

Files:

- `src/PixelToRayWidget.h`
- `src/PixelToRayWidget.cpp`

Changes:

- Added `appendTexturedCylinderTriangle(...)`.
- Added a `texturedCylinderSurfaceVertices` mesh in `paintGL()`.
- Threaded the textured mesh through `appendPanoramaBand(...)` and `appendXTangentCylinder(...)`.
- For each existing CPU-colored cylinder triangle, also emit a matching `TexturedCylinderVertex` triangle.
- The textured mesh uses the positive/negative cylinder fill colors as shader fallback colors.
- The textured mesh is not drawn yet, so visible output should still match the previous step.

Next step:

- Add a textured-cylinder draw function that can render `texturedCylinderSurfaceVertices` with the shader and texture, then keep it behind a local disabled switch until visual comparison is ready.

### Step 5: Add Disabled Textured-Cylinder Draw Path

Status: done.

Files:

- `src/PixelToRayWidget.h`
- `src/PixelToRayWidget.cpp`

Changes:

- Added `drawTexturedCylinderVertices(...)`.
- Added `kUseTexturedCylinderShader = false` as the local switch for the inactive shader path.
- Wired the surface draw stage so the textured-cylinder path can replace the CPU-colored cylinder path by flipping that switch.
- Bound the fisheye `QOpenGLTexture` when available.
- Set shader uniforms for MVP, inverse globe orientation, cap angle, projection circle, and projection image size.
- Kept the current CPU-colored cylinder draw path active by default.

Next step:

- Temporarily enable the textured path for local visual validation, fix coordinate or texture orientation issues, then decide whether to make it the default.

### Step 6: Enable Textured-Cylinder Shader Path

Status: done.

Files:

- `src/PixelToRayWidget.cpp`
- `017-X_TANGENTIAL_CYLINDER_NOTE.md`
- `016-IMAGE_TO_GLOBE_MAPPING_NOTE.md`
- `009-USER_MANUAL.md`

Changes:

- Set `kUseTexturedCylinderShader = true`.
- The cylinder surface now draws with `drawTexturedCylinderVertices(...)`.
- The old CPU-colored cylinder mesh is still generated as a fallback/comparison path, but it is not the active draw path while the switch is true.
- The line traces, hatch overlays, companion circular trace, and handle overlays still use the existing color-only draw path.
- Runtime launch for five seconds produced no shader-link or runtime OpenGL warning output beyond the existing Wayland notice.

Follow-up:

- Visual inspection is still needed to confirm texture orientation and projection alignment.
- If the texture appears vertically flipped, adjust either the image upload orientation or the shader `texV` calculation.

## Files Expected To Change

Likely implementation files:

- `src/PixelToRayWidget.h`
- `src/PixelToRayWidget.cpp`

Possible supporting documentation updates:

- `017-X_TANGENTIAL_CYLINDER_NOTE.md`
- `016-IMAGE_TO_GLOBE_MAPPING_NOTE.md`
- `014-VIEWPORT_DRAW_SEQUENCE_NOTES.md`
- `009-USER_MANUAL.md`

## Validation Checklist

The refactor should be considered correct when:

- the cylinder wall no longer shows large flat color patches
- rotating the global orientation updates the cylinder texture consistently
- changing latitude, longitude, roll, `positive_z`, `positive_y`, or `negtive_y` updates the projected texture coherently
- the positive and negative cylinder halves still use the correct fallback colors when needed
- surface-depth hiding still changes the cylinder depth behavior
- guide lines and endpoint markers still draw above the cylinder as before
- no-image startup still renders a usable cylinder
- the build passes

## Open Decisions

Before implementation, make these choices explicit:

1. Keep nearest filtering for strict parity, or switch to linear filtering for smoother appearance.
2. Flip the uploaded texture data or flip `texV` in the shader.
3. Keep one textured-cylinder VBO or reuse the current VBO with a separate vertex format.
4. Keep the globe on CPU color sampling for now, or plan a later globe shader texture pass.
5. Keep `140 degrees` hard-coded for parity, or expose the cap angle as a shared renderer setting.

## Summary

The refactor should move the cylinder texture lookup from CPU triangle centroids to a dedicated fragment shader.

The first safe architecture is:

1. CPU builds the same cylinder shape.
2. CPU uploads the fisheye image as a texture.
3. Vertex shader passes world position to the fragment shader.
4. Fragment shader projects that world position into the fisheye image.
5. Fragment shader samples the texture or falls back to the current fill color.

This keeps the current interaction model intact while removing the main source of coarse cylinder texture patches.
