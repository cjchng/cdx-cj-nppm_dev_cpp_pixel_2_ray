# Globe Orientation Refactor Notes

## Problem Summary

The previous implementation treated global-axis endpoint dragging as if it should directly update the local frame state:

- `latitude`
- `longitude`
- `roll`

That approach rotates the local `x-y-z` frame only.

It does **not** rotate the globe itself.

This is why the interaction felt wrong.

## Key Distinction

There are two different axis systems in this app:

### `X-Y-Z`

These are the fixed global/world axes.

- `X = (1, 0, 0)`
- `Y = (0, 1, 0)`
- `Z = (0, 0, 1)`

They are used in [appendGlobalAxisTraces](/Users/cj/Documents/cdx-cj-nppm_dev_cpp_pixel_2_ray/src/PixelToRayWidget.cpp:1006) and should remain fixed when the globe rotates.

### `x-y-z`

These are the local axes attached to the globe anchor point.

They are derived from:

- `latitude`
- `longitude`
- `roll`

in [FrameMath::getLocalFrame](/Users/cj/Documents/cdx-cj-nppm_dev_cpp_pixel_2_ray/src/FrameMath.cpp:54).

So:

- `X-Y-Z` = fixed world reference
- `x-y-z` = moving local frame on the globe

## Why The Old Approach Was Wrong

The old drag logic updated `latitude/longitude/roll` directly, which means:

- the local frame changed
- the anchor changed
- but the globe geometry itself stayed in the same orientation

That is not “rotate the globe”.

## Refactor Policy

The correct model is:

1. Keep `X-Y-Z` fixed in world space.
2. Introduce an explicit globe orientation state.
3. Rotate the globe geometry by that state.
4. Rotate the anchor point and local `x-y-z` frame by that same state.
5. Keep `latitude/longitude/roll` as globe-local coordinates, not world coordinates.

## New State

The refactor adds:

```cpp
QQuaternion m_globeOrientation;
```

This quaternion represents the globe-to-world rotation.

## Core Refactor Code

### 1. Rotate the anchor point

Before:

```cpp
QVector3D PixelToRayWidget::currentOrigin() const {
    return FrameMath::latLonToVector(m_state.latitude, m_state.longitude, m_globeRadius);
}
```

After:

```cpp
QVector3D PixelToRayWidget::currentOrigin() const {
    return m_globeOrientation.rotatedVector(
        FrameMath::latLonToVector(m_state.latitude, m_state.longitude, m_globeRadius)
    );
}
```

### 2. Rotate the local frame

Before:

```cpp
FrameMath::Frame PixelToRayWidget::currentFrame() const {
    return FrameMath::getLocalFrame(m_state.latitude, m_state.longitude, m_state.roll);
}
```

After:

```cpp
FrameMath::Frame PixelToRayWidget::currentFrame() const {
    const FrameMath::Frame localFrame =
        FrameMath::getLocalFrame(m_state.latitude, m_state.longitude, m_state.roll);
    return {
        m_globeOrientation.rotatedVector(localFrame.xAxis).normalized(),
        m_globeOrientation.rotatedVector(localFrame.yAxis).normalized(),
        m_globeOrientation.rotatedVector(localFrame.zAxis).normalized()
    };
}
```

### 3. Rotate sphere geometry

Before:

```cpp
const QVector3D p00 = sphericalPoint(m_globeRadius, lat0, lon0);
```

After:

```cpp
const QVector3D p00 =
    m_globeOrientation.rotatedVector(sphericalPoint(m_globeRadius, lat0, lon0));
```

The same rule must be applied to:

- sphere surface
- sphere wireframe
- any other globe-attached geometry that is generated in globe-local coordinates

### 4. Convert world picks back to globe-local coordinates

When tracking the sphere surface, the mouse ray hits a world-space point on the rotated globe.

That point must be converted back into globe-local coordinates before computing `latitude/longitude`:

```cpp
const QVector3D globeLocalPoint =
    m_globeOrientation.conjugated().rotatedVector(spherePoint);
vectorToLatLon(globeLocalPoint, latitude, longitude);
```

## Global Endpoint Drag Policy

Global endpoint drag should rotate the globe, not rewrite the local frame directly.

Correct drag model:

1. User picks a fixed world-space endpoint on the sphere, for example `+X`.
2. Store:
   - the picked world-space direction
   - the starting globe orientation
3. As the user drags, compute the new target world-space sphere direction.
4. Build a delta rotation from the start direction to the target direction.
5. Apply:

```cpp
m_globeOrientation = (delta * startOrientation).normalized();
```

This rotates the whole globe while leaving the global `X-Y-Z` frame fixed.

## Result

After the refactor:

- dragging global endpoints rotates the globe
- the globe center stays fixed
- the global `X-Y-Z` reference stays fixed
- the local `x-y-z` frame follows the rotated globe correctly
- saved/default mode switching can preserve globe orientation if the quaternion is persisted

## Files Affected

- [src/PixelToRayWidget.h](/Users/cj/Documents/cdx-cj-nppm_dev_cpp_pixel_2_ray/src/PixelToRayWidget.h)
- [src/PixelToRayWidget.cpp](/Users/cj/Documents/cdx-cj-nppm_dev_cpp_pixel_2_ray/src/PixelToRayWidget.cpp)
- [src/MainWindow.h](/Users/cj/Documents/cdx-cj-nppm_dev_cpp_pixel_2_ray/src/MainWindow.h)
- [src/MainWindow.cpp](/Users/cj/Documents/cdx-cj-nppm_dev_cpp_pixel_2_ray/src/MainWindow.cpp)
