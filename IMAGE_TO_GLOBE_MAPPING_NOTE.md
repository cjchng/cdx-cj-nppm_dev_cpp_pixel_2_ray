# Image To Globe Mapping Note

This note describes how a fisheye image is mapped onto the globe in the current implementation.

## Scope

The mapping code lives in:

- [src/PixelToRayWidget.cpp](/Users/cj/Documents/cdx-cj-nppm_dev_cpp_pixel_2_ray/src/PixelToRayWidget.cpp:1134)
- [src/PixelToRayWidget.h](/Users/cj/Documents/cdx-cj-nppm_dev_cpp_pixel_2_ray/src/PixelToRayWidget.h:198)

The active pipeline is:

1. load the fisheye image
2. detect the valid circular image region
3. inverse-map each globe surface point into that image
4. sample the image color
5. use that sampled color as the globe surface color

## 1. Image Load

The widget loads the fisheye image through:

- `loadProjectionImage(const QString &path)`

Implementation:

- the file is loaded into `QImage`
- it is converted to `QImage::Format_RGBA8888`
- the absolute file path is stored in `m_projectionImagePath`
- `detectProjectionCircle()` is called immediately afterward

Relevant members:

- `m_projectionImage`
- `m_projectionImagePath`
- `m_projectionCircleCenter`
- `m_projectionCircleRadius`

## 2. Valid Circular Border Extraction

The code does not assume the valid fisheye disc exactly fills the image. It estimates the valid circular region first in:

- `detectProjectionCircle()`

Current method:

1. sample small patches from the four corners
2. average those patches into a background color
3. classify a pixel as foreground if its color is sufficiently different from that background
4. scan inward from left, right, top, and bottom near the image center
5. average those scan results into:
   - `m_projectionCircleCenter`
   - `m_projectionCircleRadius`

Important detail:

- if the estimated radius is too small, the code falls back to a centered circle with radius `min(width, height) / 2`

This is what lets `circular.png` or `Entaniya 280-Facing Up.jpg` be used without hard-coding a perfect circle center or diameter.

## 3. Globe Point To Image Pixel Mapping

The main inverse mapping is implemented in:

- `sampleProjectionColor(const QVector3D &worldPoint) const`

For a globe surface point:

1. convert the world-space point back into globe-local space
2. compute its polar angle from the local `+z` axis
3. reject points outside the allowed cap
4. convert that polar angle into a normalized fisheye radius
5. convert the local in-plane direction into image `x/y`
6. sample the image at that pixel location

### 3.1 World Space Back To Globe-Local Space

The globe may already be rotated by `m_globeOrientation`, so mapping must first undo that rotation:

```cpp
const QVector3D globeLocal =
    m_globeOrientation.conjugated().rotatedVector(worldPoint).normalized();
```

This is important because the fisheye image is defined in globe-local coordinates, not camera/world coordinates.

### 3.2 Cap Angle

The mapping currently assumes an equisolid-angle fisheye with:

```cpp
constexpr float kEntaniyaCapAngleDegrees = 140.0f;
```

The code computes:

- `thetaRadians = acos(globeLocal.z())`
- `thetaDegrees = thetaRadians * 180 / pi`

If `thetaDegrees > 140`, the point is outside the valid fisheye cap and returns an invalid `QColor`.

### 3.3 Equisolid-Angle Radius

The normalized image-disc radius is computed as:

```cpp
radiusNormalized =
    sin(0.5 * thetaRadians) / sin(0.5 * capHalfRadians);
```

where:

- `thetaRadians` is the angular distance from the local `+z` axis
- `capHalfRadians = 0.5 * 140°`

This is the inverse equisolid-angle mapping used by the current code.

### 3.4 Image Coordinates

The local in-plane direction comes from the normalized `x/y` components:

```cpp
dx = globeLocal.x() / xyLength
dy = globeLocal.y() / xyLength
```

The final image coordinates are:

```cpp
pixelX = circleCenter.x + radiusNormalized * circleRadius * dx;
pixelY = circleCenter.y - radiusNormalized * circleRadius * dy;
```

The `-dy` is intentional because image `y` grows downward while globe-local `+y` is treated upward in the mapping plane.

## 4. Globe Sampling Method

The globe surface uses bicubic interpolation in:

- `sampleProjectionColor(...)`

The implementation:

1. find the surrounding integer base pixel
2. compute fractional offsets `tx` and `ty`
3. fetch a `4 x 4` neighborhood
4. interpolate each row with `catmullRom(...)`
5. interpolate those row results again with `catmullRom(...)`

So the globe uses a Catmull-Rom bicubic reconstruction rather than nearest or bilinear sampling.

This is why the globe mapping looks smoother than the earlier versions.

## 5. Globe Rendering Path

The sphere surface is built in:

- `appendSphereSurface(std::vector<Vertex> &vertices) const`

For each sphere vertex:

1. generate a globe-local sphere point with `sphericalPoint(...)`
2. rotate it by `m_globeOrientation`
3. call `sampleProjectionColor(point)`
4. if the sample is valid, use the sampled image color
5. otherwise fall back to the shaded procedural globe color

Relevant line:

```cpp
const QColor projectedColor = sampleProjectionColor(point);
```

This means the image texture is not a GPU texture object with UVs. It is currently CPU-side per-vertex color generation driven by inverse projection sampling.

## 6. Mesh Density

When no fisheye image is loaded, the globe uses a coarse mesh:

- `latBands = 28`
- `lonBands = 48`

When an image is loaded, the globe uses a finer mesh:

- `latBands = 256`
- `lonBands = 512`

This higher tessellation is necessary because the mapped image is baked into per-vertex colors rather than sampled in a fragment shader.

## 7. Cylinder Variant

The x-tangential cylinder now uses a shader sampling path:

- `drawTexturedCylinderVertices(...)`

Differences from the globe:

- same inverse globe-local and equisolid-angle mapping
- same extracted circle center/radius
- sampling happens per fragment from an uploaded fisheye `QOpenGLTexture`
- the texture uses nearest filtering for parity with the previous nearest-pixel cylinder behavior

The previous CPU helper, `sampleProjectionColorNearest(...)`, is still present, but the active cylinder surface path uses shader lookup while `kUseTexturedCylinderShader` is enabled.

## 8. Default Image Source

The built-in default fisheye source is:

- `circular.png`

The widget also falls back to:

- `Entaniya 280-Facing Up.jpg`

if `circular.png` is not available.

The startup/preset profile logic may override that with a user-selected image path from:

- `assets/preset_state.json`
- `assets/current_state.json`

## 9. Current Limitations

The current mapping method has several practical limits:

- it is per-vertex color mapping, not true texture UV sampling in the shader
- globe quality still depends on sphere tessellation density
- the `140°` cap is fixed in code
- the circle extraction is heuristic, not a robust calibrated lens-fit
- the globe uses CPU bicubic sampling, while the cylinder uses shader texture sampling with nearest filtering
- projection type, image flips, and calibrated pixel-to-ray maps are not yet profile-controlled

## 10. Next Development Plan

The next mapping work is planned in [FISHEYE_GLOBE_MAPPING_PLAN.md](FISHEYE_GLOBE_MAPPING_PLAN.md).

That plan covers:

- multiple projection models: equisolid, equidistant, stereographic, orthographic, rectilinear, and pixel-ray map
- profile fields for fisheye projection, cap angle, manual circle override, and horizontal/vertical flip
- calibrated pixel-to-spatial-ray discrete maps and the required inverse lookup from globe ray to image pixel
- keeping CPU and GPU sampling behavior aligned

## 11. Summary

The current image-to-globe mapping is:

- inverse equisolid-angle projection
- based on an extracted valid fisheye disc
- evaluated in globe-local coordinates
- sampled bicubically for the globe
- applied as per-vertex colors on a dense sphere mesh

That is the implementation that is active today.
