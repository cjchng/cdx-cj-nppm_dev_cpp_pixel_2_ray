# Global Center Policy

## Purpose

This note documents the current `Global center (fix/move)` behavior, how it relates to `Globe Orientation`, the rendering inconsistency identified during review, and the recommended policy for future implementation.

## Current UI Meaning

The viewport context menu exposes:

- `Global center (fix/move)`

Intended user meaning:

- `fix`: keep the camera centered on the world origin
- `move`: keep the camera centered on the current anchor point on the globe

## Current Technical Problem

The code currently mixes two different concepts:

1. the physical center of the sphere
2. the camera target used by the view transform

These are not the same thing.

## Globe Orientation Contract

`Globe Orientation` is a rotation-only transform. It changes how globe-local coordinates are placed in world space, but it does not move the physical globe center.

The runtime value is the normalized quaternion:

```cpp
QQuaternion m_globeOrientation;
```

Conceptually:

```cpp
worldPoint = m_globeOrientation.rotatedVector(globeLocalPoint);
globeLocalPoint = m_globeOrientation.conjugated().rotatedVector(worldPoint);
```

Important consequences:

- the physical sphere center remains world `(0, 0, 0)`
- the quaternion rotates the globe surface, local anchor, local frame axes, globe-axis traces, and texture sampling basis
- the quaternion must not be interpreted as a translation or as a replacement for the global center
- `Global q_w/q_x/q_y/q_z` and the read-only Euler axis-angle values describe the same rotation, not a camera target and not a yaw/pitch/roll position

The current anchor is therefore a rotated point on the sphere:

```cpp
currentOrigin() =
    m_globeOrientation.rotatedVector(
        FrameMath::latLonToVector(latitude, longitude, globeRadius));
```

The local frame is also rotated by the same quaternion:

```cpp
currentFrame().xAxis = m_globeOrientation.rotatedVector(localFrame.xAxis);
currentFrame().yAxis = m_globeOrientation.rotatedVector(localFrame.yAxis);
currentFrame().zAxis = m_globeOrientation.rotatedVector(localFrame.zAxis);
```

This means local `z` is the oriented radial direction through the current anchor. The `va` display in Local Components is an angular equivalent of `positive_y` at the true global center:

```text
va = atan2(positive_y, globe_radius)
```

That angle is meaningful only because the local frame is constructed from a sphere centered at world origin. Changing `Globe Orientation` rotates the whole construction around that origin; it does not change the origin itself.

### What Is Correct Today

The sphere geometry is generated around world origin, then oriented by `m_globeOrientation`.

That means the true sphere center is:

```cpp
QVector3D(0.0f, 0.0f, 0.0f)
```

This assumption is also used by the mouse-to-sphere intersection code.

It is also required by:

- globe texture sampling, which maps a world point back through `m_globeOrientation.conjugated()`
- globe-axis terminal markers and traces, which rotate canonical globe axes around world origin
- local component geometry, which starts at `currentOrigin()` and extends along rotated local axes

### What Goes Wrong In `move` Mode

In `move` mode, the code currently lets the camera target become the current anchor point. That is reasonable for camera behavior.

However, the same value is also reused as if it were the sphere center for:

- sphere shading normals
- front/rear wireframe classification
- some camera-facing line geometry calculations

That causes rendering errors because the sphere mesh itself did not move.

With `Globe Orientation` enabled, the error is easier to trigger and harder to reason about: `currentOrigin()` is a rotated surface point, so treating it as the center makes lighting, rear/front tests, and guide geometry depend on the selected anchor and quaternion rather than on the actual globe center.

## Observable Consequences

When `Global center` is set to `move`, the following can become visually wrong:

- sphere shading can look skewed
- rim lighting can drift
- front hemisphere vs rear hemisphere wireframe classification can be incorrect

This is a modeling bug, not just a cosmetic preference issue.

## Recommended Policy

The implementation should separate:

### 1. Sphere Center

This should remain fixed, independent of `Global center (fix/move)` and independent of `Globe Orientation`:

```cpp
QVector3D(0.0f, 0.0f, 0.0f)
```

Use it for:

- sphere surface normals
- front/rear hemisphere tests
- ray/sphere intersection
- any geometry that truly depends on the globe's physical center
- angular derivations such as `va`
- any mapping from world-space geometry back to globe-local coordinates

### 2. Camera Target

This should depend on the menu mode:

- `fix`: world origin
- `move`: current anchor point

Use it for:

- `lookAt(...)`
- camera-relative navigation logic
- any view framing behavior

`Globe Orientation` should not change this policy. It may rotate `currentOrigin()`, and `move` mode may choose that rotated anchor as the camera target, but that target is still only a camera target. It is not the sphere center.

### 3. Globe Orientation

This should remain a separate rotation state:

```cpp
QQuaternion globeOrientation() const;
```

Use it for:

- rotating globe-local points into world space
- rotating local frame axes into world space
- inverse-mapping world points into globe-local coordinates for texture lookup and interaction
- deriving the displayed axis-angle equivalent

Do not use it for:

- deciding whether the physical sphere center changed
- choosing the camera target directly
- replacing the world origin in geometry tests

## Recommended Refactor

Instead of one overloaded function such as:

```cpp
currentSceneCenter()
```

split the logic into two explicit helpers:

```cpp
QVector3D sphereCenter() const;
QVector3D cameraTarget() const;
```

Suggested meanings:

- `sphereCenter()`: always return world origin
- `cameraTarget()`: return world origin or `currentOrigin()` depending on `fixGlobalCenter`

Keep `m_globeOrientation` separate from both helpers. A good naming rule is:

- functions containing `center` should return a position
- functions containing `orientation` should return or apply a rotation
- functions containing `target` should describe camera framing

Those names should not be interchangeable.

## Suggested Code Ownership

The following code paths should use `sphereCenter()`:

- sphere shading in `appendSphereSurface(...)`
- wireframe visibility classification in `appendSphereWireframe(...)`
- screen-to-sphere mapping in `tryMapScreenToSphere(...)`
- globe-local conversion for image sampling
- globe-axis endpoint front/rear classification
- local-component angular derivations such as `va`

The following code paths should use `cameraTarget()`:

- `updateViewProjection(...)`
- camera-facing thick-line orientation helpers if they conceptually depend on the view target

The following code paths should use `m_globeOrientation` or explicit orientation helpers:

- `currentOrigin()`
- `currentFrame()`
- globe texture sampling inverse transforms
- globe-axis trace and endpoint generation
- the `Globe Orientation` panel and its read-only axis-angle display

## Recovery / Git Note

At the time of writing this note, the repository worktree is dirty and contains both user edits and assistant edits.

Important consequence:

- a plain git tag can only point to a committed revision
- it cannot capture uncommitted working-tree changes by itself

If you want a reliable recovery point for the exact current state, the safest workflow is:

1. create a recovery branch from the current worktree
2. commit the current state on that branch
3. optionally add a tag to that recovery commit

That gives you a fully restorable snapshot, including uncommitted edits.
