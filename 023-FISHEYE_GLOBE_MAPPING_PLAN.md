# Fisheye Image To Globe Mapping Development Plan

This document defines the current mapping contract and the next development plan for mapping fisheye image pixels onto the globe surface.

The goal is to support three future needs without breaking the current behavior:

1. multiple fisheye projection models
2. calibrated pixel-to-spatial-ray discrete maps
3. optional horizontal or vertical image flips before the image is used

## Current Mapping Contract

The active implementation is documented in [016-IMAGE_TO_GLOBE_MAPPING_NOTE.md](016-IMAGE_TO_GLOBE_MAPPING_NOTE.md).

The important current rule is that rendering is an inverse lookup:

```text
globe surface point -> globe-local ray direction -> fisheye image pixel -> sampled color
```

The fisheye image is not currently unwrapped into an intermediate panorama. The app samples the original fisheye image by projecting each globe-local direction into the detected fisheye disc.

Current code paths:

- `PixelToRayWidget::setFisheyeImagePath(...)`
- `PixelToRayWidget::loadProjectionImage(...)`
- `PixelToRayWidget::detectProjectionCircle()`
- `PixelToRayWidget::sampleProjectionColor(...)`
- `PixelToRayWidget::sampleProjectionColorNearest(...)`
- `PixelToRayWidget::appendSphereSurface(...)`
- `PixelToRayWidget::drawTexturedCylinderVertices(...)`

Current state:

- the globe surface uses CPU bicubic image sampling and bakes the sampled color into dense sphere vertices
- the x-tangential cylinder uses a GPU shader path and samples the uploaded fisheye texture per fragment
- both paths use the same conceptual mapping from globe-local direction to fisheye image pixel
- the current projection formula is equisolid-angle style
- the current valid cap angle is hard-coded as `kEntaniyaCapAngleDegrees = 140.0f`

## Coordinate Spaces

Use these coordinate spaces consistently:

- image pixel space: `px = (x, y)`, origin at the top-left image pixel
- fisheye disc space: detected center `C = (cx, cy)` and radius `R`
- globe-local space: the image optical axis is local `+Z`
- world space: OpenGL scene coordinates

The globe orientation quaternion maps globe-local directions into world space:

```text
worldDir = globeOrientation * globeLocalDir
```

Texture lookup must undo that rotation:

```text
globeLocalDir = inverse(globeOrientation) * worldDir
```

For the current sphere centered at world origin:

```text
worldDir = normalize(worldPoint)
```

If a future implementation allows the physical sphere center to move, the direction must become:

```text
worldDir = normalize(worldPoint - sphereCenter)
```

The `Global Center` view option is a camera targeting policy. It must not be treated as the physical sphere center.

## Current Fisheye Disc Detection

The current implementation estimates a valid fisheye disc from the loaded image:

1. sample the four image corners
2. average them into a background color
3. classify pixels as foreground when color distance from the background exceeds a threshold
4. scan near the image center from left, right, top, and bottom
5. compute the disc center and radius from those detected borders
6. fall back to a centered disc if the estimated radius is invalid

This is practical for the current images, but it is heuristic. The next implementation should keep auto-detection as a default and add optional manual override fields:

```json
{
  "fisheye_circle_center_x": null,
  "fisheye_circle_center_y": null,
  "fisheye_circle_radius": null
}
```

`null` means auto-detect. Numeric values mean use the explicit calibration.

## Projection Model Support

The current mapping converts a globe-local unit ray:

```text
d = (x, y, z)
theta = acos(clamp(z, -1, 1))
phi = atan2(y, x)
```

into a normalized image radius:

```text
rn = radial(theta) / radial(thetaMax)
```

Then pixel coordinates are:

```text
dx = x / sqrt(x*x + y*y)
dy = y / sqrt(x*x + y*y)

pixelX = cx + rn * R * dx
pixelY = cy - rn * R * dy
```

The `pixelY` sign is negative because image `+Y` points downward, while globe-local `+Y` points upward in the fisheye plane.

### Projection Model Table

Add a profile field:

```json
{
  "fisheye_projection_model": "equisolid",
  "fisheye_cap_angle_deg": 140.0
}
```

Recommended projection models:

| Model | `radial(theta)` | Notes |
| --- | --- | --- |
| `equisolid` | `sin(theta / 2)` | Current behavior. Common for many fisheye lenses. |
| `equidistant` | `theta` | Common action-camera approximation. |
| `stereographic` | `tan(theta / 2)` | Preserves circles better than some other fisheye models. |
| `orthographic` | `sin(theta)` | Valid mainly up to 90 deg from the optical axis. |
| `rectilinear` | `tan(theta)` | Pinhole camera model. Invalid near 90 deg. |

For all models:

```text
if theta > thetaMax: sample is invalid
if rn > 1: sample is invalid
```

For `orthographic` and `rectilinear`, the UI should warn or clamp when `thetaMax` exceeds the mathematically stable domain.

### Implementation Rule

Do not duplicate projection formulas independently in CPU and GLSL code.

Use a small shared conceptual API:

```cpp
enum class FisheyeProjectionModel {
    Equisolid,
    Equidistant,
    Stereographic,
    Orthographic,
    Rectilinear,
    PixelRayMap
};

float normalizedFisheyeRadius(float thetaRadians,
                              float thetaMaxRadians,
                              FisheyeProjectionModel model);
```

Then mirror the same switch in the shader. Keep a CPU test table that verifies model outputs for known angles so CPU and GPU stay aligned.

## Pixel-To-Spatial-Ray Discrete Mapping

Some lenses cannot be described accurately by a simple radial projection. For those, support a calibrated discrete map:

```text
image pixel -> spatial ray direction
```

Example conceptual record for each pixel:

```json
{
  "x": 100,
  "y": 200,
  "ray": [0.123, -0.456, 0.881],
  "valid": true
}
```

For production use, the file should not be a huge JSON array. The plan is:

1. use a small JSON metadata sidecar
2. store the dense ray map in a compact binary or image-like array format
3. keep JSON-only test maps for unit tests and small fixtures

Recommended metadata:

```json
{
  "type": "pixel_to_ray_map",
  "version": 1,
  "image_width": 1920,
  "image_height": 1920,
  "coordinate_space": "globe_local",
  "ray_order": "xyz",
  "ray_file": "lens_map.f32",
  "valid_mask_file": "lens_mask.u8"
}
```

### Important Inversion Problem

Rendering the globe needs the opposite lookup:

```text
globe-local ray direction -> image pixel
```

If the calibration file only provides `pixel -> ray`, the app must build an inverse lookup.

Recommended staged approach:

1. Load all valid pixel rays.
2. Normalize every ray.
3. Build a search structure keyed by direction, such as a coarse spherical grid plus nearest angular search.
4. For each requested globe-local direction, find the nearest valid pixel ray.
5. Optionally blend the nearest 3 or 4 angular neighbors to reduce blockiness.

CPU-first acceptance is acceptable for the first step. GPU support can follow later by precomputing a `direction -> uv` atlas or by uploading a lookup texture matched to the sphere mesh.

### Discrete Map Profile Fields

Add profile fields:

```json
{
  "fisheye_projection_model": "pixel_ray_map",
  "fisheye_pixel_ray_map_path": "maps/entaniya_280.json",
  "fisheye_pixel_ray_map_interpolation": "nearest"
}
```

Recommended interpolation values:

- `nearest`
- `angular_linear`
- `barycentric`

For the first implementation, `nearest` is sufficient if we clearly mark it as a calibration preview mode.

## Horizontal And Vertical Flip

Add profile fields:

```json
{
  "fisheye_flip_horizontal": false,
  "fisheye_flip_vertical": false
}
```

The user intent is to flip the source image before mapping. In implementation terms, there are two valid strategies.

### Strategy A: Flip Runtime Image After Decode

After loading the source image and before circle detection:

```cpp
image = image.mirrored(flipHorizontal, flipVertical);
```

Then:

1. detect the fisheye circle on the flipped runtime copy
2. upload the flipped runtime copy as the OpenGL texture
3. sample normally

This is the simplest and least error-prone plan. It does not modify the source file on disk.

### Strategy B: Transform Pixel Coordinates At Sample Time

Keep the decoded image unchanged and transform the sampled pixel:

```text
if flipHorizontal: pixelX = imageWidth  - 1 - pixelX
if flipVertical:   pixelY = imageHeight - 1 - pixelY
```

This is useful if we need to preserve the original image buffer for comparison, but it requires careful parity between CPU and GPU sampling.

### Recommended First Step

Use Strategy A for normal projection images.

For pixel-to-ray maps, flip the image and map together. A horizontal image flip means:

```text
mapRayAtFlippedPixel(x, y) = originalRay(width - 1 - x, y)
```

A vertical image flip means:

```text
mapRayAtFlippedPixel(x, y) = originalRay(x, height - 1 - y)
```

Do not flip the image and then also flip sampled pixel coordinates. That would double-apply the correction.

## Data Model Plan

Extend Current and Preset profiles with:

```json
{
  "fisheye_projection_model": "equisolid",
  "fisheye_cap_angle_deg": 140.0,
  "fisheye_flip_horizontal": false,
  "fisheye_flip_vertical": false,
  "fisheye_circle_center_x": null,
  "fisheye_circle_center_y": null,
  "fisheye_circle_radius": null,
  "fisheye_pixel_ray_map_path": "",
  "fisheye_pixel_ray_map_interpolation": "nearest"
}
```

Backward compatibility:

- missing `fisheye_projection_model` means `equisolid`
- missing `fisheye_cap_angle_deg` means the current `140.0`
- missing flip fields mean `false`
- missing circle override fields mean auto-detect
- missing map path means no map

## UI Plan

Add a compact `Image Mapping` subsection in the browser UI Server Image panel or Globe Coordinates panel.

Suggested fields:

- `Projection`: menu, default `equisolid`
- `Cap Angle`: numeric field in degrees
- `Flip H`: toggle
- `Flip V`: toggle
- `Circle`: auto/manual segmented control
- `Circle X`, `Circle Y`, `Radius`: enabled only when manual circle mode is active
- `Pixel-Ray Map`: file picker, enabled only when `Projection = pixel_ray_map`
- `Map Interpolation`: menu, enabled only when `Projection = pixel_ray_map`

The Qt app right-side table should expose the same profile fields after the browser workflow is accepted.

## Refactor Plan

### Step 1: Extract A Mapping Helper

Create a helper that owns projection state and maps directions to image pixels:

```cpp
struct FisheyeMappingConfig {
    FisheyeProjectionModel model;
    float capAngleDeg;
    bool flipHorizontal;
    bool flipVertical;
    std::optional<QPointF> circleCenterOverride;
    std::optional<float> circleRadiusOverride;
    QString pixelRayMapPath;
};

std::optional<QPointF> mapGlobeLocalDirectionToPixel(const QVector3D &globeLocalDir,
                                                     const FisheyeMappingConfig &config,
                                                     const ProjectionCircle &circle,
                                                     const QSize &imageSize);
```

Keep this helper independent from UI code.

### Step 2: Preserve Current Behavior As Default

Before adding options, route the existing `sampleProjectionColor(...)`, `sampleProjectionColorNearest(...)`, and cylinder shader uniforms through the new config with:

```text
model = equisolid
capAngleDeg = 140
flipHorizontal = false
flipVertical = false
circleOverride = none
pixelRayMapPath = empty
```

Acceptance: existing profiles render the same.

### Step 3: Add Projection Models

Add the radial model switch in CPU code and shader code.

Acceptance:

- `equisolid` remains visually unchanged
- synthetic axis test image maps center and cardinal directions consistently
- CPU and shader paths agree for the same `theta`

### Step 4: Add Flip Controls

Apply runtime image flip immediately after decode and before circle detection/texture upload.

Acceptance:

- horizontal flip swaps left/right markers on the globe
- vertical flip swaps up/down markers on the globe
- both flips together are equivalent to 180 deg rotation in image pixel space, not a globe orientation change

### Step 5: Add Manual Circle Override

Use auto-detected circle by default. If all manual circle fields are present and valid, use the manual circle.

Acceptance:

- manual values are persisted in Current/Preset
- invalid manual values fall back to auto-detection with a visible status warning

### Step 6: Add Pixel-Ray Map Preview

Implement CPU lookup first.

Acceptance:

- load a small JSON test map
- render nearest-neighbor mapped colors on the globe
- show a clear warning if image size and ray-map size do not match
- disable shader cylinder texture map or use CPU fallback until a GPU-compatible inverse map exists

### Step 7: Unify CPU And GPU Sampling

After projection settings are stable, decide whether to:

1. keep the globe CPU-colored and update only the cylinder shader, or
2. move globe sampling into a shader as well

Long term, shader sampling is preferred because it avoids very dense globe tessellation and keeps projection behavior continuous per fragment.

## Testing Plan

Add synthetic fixture images:

- center dot at optical axis
- cardinal labels at image right, left, top, bottom
- concentric rings with known angular spacing
- asymmetric text image for flip verification

Add logic tests for:

- each projection model at `theta = 0`, `thetaMax / 2`, and `thetaMax`
- cap rejection above `thetaMax`
- horizontal and vertical pixel coordinate transforms
- circle override validation
- pixel-ray map nearest lookup
- globe orientation inverse mapping

Manual visual checks:

- compare globe and cylinder texture positions for the same image
- rotate the globe orientation quaternion and verify the texture remains attached to the globe surface
- switch Current/Preset and verify projection fields follow the selected profile

## Open Decisions

1. Should projection settings live in `Image`, `Globe Coordinates`, or a new `Image Mapping` panel?
2. Should `cap angle` replace the older `Cap Angle` viewport clipping field, or remain a separate image-lens field?
3. Should manual circle override be exposed immediately or kept as an advanced setting?
4. Which pixel-ray map file format should be accepted first for real calibration data?
5. Should the first pixel-ray map implementation disable the textured cylinder shader until the inverse map is GPU-ready?

## Recommended Next Development Order

1. Add profile fields with defaults and UI display only.
2. Extract the CPU mapping helper while preserving current visuals.
3. Add projection model selection.
4. Add horizontal and vertical flip.
5. Add manual circle override.
6. Add pixel-ray map loader and CPU nearest inverse lookup.
7. Revisit GPU shader unification after the data model is stable.
