# Global Center Policy

## Purpose

This note documents the current `Global center (fix/move)` behavior, the rendering inconsistency identified during review, and the recommended policy for future implementation.

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

### What Is Correct Today

The sphere geometry is generated around world origin.

That means the true sphere center is:

```cpp
QVector3D(0.0f, 0.0f, 0.0f)
```

This assumption is also used by the mouse-to-sphere intersection code.

### What Goes Wrong In `move` Mode

In `move` mode, the code currently lets the camera target become the current anchor point. That is reasonable for camera behavior.

However, the same value is also reused as if it were the sphere center for:

- sphere shading normals
- front/rear wireframe classification
- some camera-facing line geometry calculations

That causes rendering errors because the sphere mesh itself did not move.

## Observable Consequences

When `Global center` is set to `move`, the following can become visually wrong:

- sphere shading can look skewed
- rim lighting can drift
- front hemisphere vs rear hemisphere wireframe classification can be incorrect

This is a modeling bug, not just a cosmetic preference issue.

## Recommended Policy

The implementation should separate:

### 1. Sphere Center

This should remain fixed:

```cpp
QVector3D(0.0f, 0.0f, 0.0f)
```

Use it for:

- sphere surface normals
- front/rear hemisphere tests
- ray/sphere intersection
- any geometry that truly depends on the globe's physical center

### 2. Camera Target

This should depend on the menu mode:

- `fix`: world origin
- `move`: current anchor point

Use it for:

- `lookAt(...)`
- camera-relative navigation logic
- any view framing behavior

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

## Suggested Code Ownership

The following code paths should use `sphereCenter()`:

- sphere shading in `appendSphereSurface(...)`
- wireframe visibility classification in `appendSphereWireframe(...)`
- screen-to-sphere mapping in `tryMapScreenToSphere(...)`

The following code paths should use `cameraTarget()`:

- `updateViewProjection(...)`
- camera-facing thick-line orientation helpers if they conceptually depend on the view target

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
