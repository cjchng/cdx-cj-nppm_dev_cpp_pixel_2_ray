# Cylinder Shader Texture Plan

This note records the planned refactor for the `x`-axis tangential cylinder texture mapping.

## Goal

Replace the current coarse triangle-color approximation with a true shader-based projective texture mapping from the globe onto the cylinder wall.

The intended projection model is:

- the global center is the common origin
- each cylinder wall point is projected back toward the globe
- the globe intersection is then mapped into the fisheye image

## Why The Current Result Is Coarse

The current implementation in [src/PixelToRayWidget.cpp](/Users/cj/Documents/cdx-cj-nppm_dev_cpp_pixel_2_ray/src/PixelToRayWidget.cpp:1597) does not compute a continuous texture.

Instead:

- each azimuth segment creates four wall triangles
- each triangle gets one sampled color from its centroid
- the positive `y` side uses only two color samples per segment
- the negative `y` side uses only two color samples per segment

So the wall is effectively a small set of flat-colored patches, not a true projected texture.

## Planned Geometry Parameterization

The cylinder wall should be parameterized by:

- `u`: azimuth around the cylinder, in `[0, 1]`
- `v`: height along local `y`, in `[0, 1]`

Definitions:

- `R = globeRadius + positive_z`
- `Hpos = positive_y * R`
- `Hneg = negtive_y * R`

Then:

```text
phi = 2*pi*u
h = lerp(-Hneg, Hpos, v)
P(u,v) = R*cos(phi)*zAxis + h*yAxis + R*sin(phi)*xAxis
```

where:

- `xAxis`
- `yAxis`
- `zAxis`

are the current local frame axes.

`P(u,v)` is the cylinder wall point in world space.

## Planned Projection Model

For each wall point `P`:

1. take the ray from the global center through `P`
2. intersect that ray with the globe
3. sample the globe image at that intersection

Because the globe center is the world origin, the globe intersection is:

```text
G = normalize(P) * globeRadius
```

This is the key simplification of the planned method.

## Planned Fisheye Mapping

Once the globe intersection `G` is known:

1. convert `G` into globe-local coordinates
2. compute the fisheye polar angle from local `+z`
3. convert that angle into equid-solid image radius
4. convert the local `x/y` direction into image coordinates

In math form:

```text
Glocal = inverse(globeOrientation) * normalize(G)
theta = acos(Glocal.z)
rho = sin(theta/2) / sin(capAngle/2)
dir = normalize(Glocal.xy)
imgX = circleCx + rho * circleR * dir.x
imgY = circleCy - rho * circleR * dir.y
```

Then normalized texture coordinates are:

```text
texU = imgX / imageWidth
texV = imgY / imageHeight
```

## Planned Shader Split

The preferred implementation split is:

### Vertex shader

- reconstruct cylinder wall position from `(u, v)` or receive world position directly
- output world position to the fragment shader

### Fragment shader

- compute `G = normalize(P) * globeRadius`
- convert `G` into globe-local coordinates
- compute fisheye UV
- sample the fisheye texture

## Why The Projection Should Be In The Fragment Shader

The projection from cylinder wall to globe image is nonlinear.

If the code computes image UV only at vertices and interpolates them across triangles:

- the distortion will be wrong
- the texture will visibly drift or flatten

So the correct planned approach is:

- keep geometry interpolation in the rasterizer
- do the projective globe lookup per fragment

## Planned Mesh Resolution

Once shader-based sampling is used, the mesh no longer needs to carry color detail.

A practical starting point is:

- azimuth segments: `144`
- height segments: `32` or `64`

That mesh is for shape continuity only. The texture detail should come from fragment sampling.

## Planned Uniforms

The shader will likely need:

- `globeRadius`
- `xCylinderRadius`
- `positiveHeight`
- `negativeHeight`
- `localFrame` or `xAxis/yAxis/zAxis`
- `inverseGlobeOrientation`
- `capAngleRadians`
- `projectionCircleCenter`
- `projectionCircleRadius`
- `projectionImageSize`
- fisheye texture sampler

## Planned CPU Refactor

The current CPU-side cylinder path should be changed from:

- centroid color sampling
- `appendTriangle(...)` with one color per triangle

to:

- generate a cylinder wall mesh with `(u, v)` structure
- upload vertex positions or cylinder parameters
- draw with a dedicated textured-cylinder shader path

This will likely require:

- a new vertex type for textured cylinder vertices
- a dedicated shader program for textured surfaces
- a `QOpenGLTexture` for the fisheye image instead of only CPU-side `QImage` sampling

## Expected Result

After the refactor:

- the cylinder wall should show a continuous texture
- the texture should follow the center-projective mapping from globe to cylinder
- the `y` direction should no longer look like only two patches above and two below
- the cylinder should visually behave as a true projected surface, not a piecewise flat-colored approximation

## Summary

The planned implementation is:

1. parameterize cylinder wall with `(u, v)`
2. render a proper cylinder wall mesh
3. project each fragment back to the globe using the global center
4. convert the globe hit to fisheye image coordinates
5. sample the texture in the fragment shader

That is the intended next-step architecture for the cylinder mapping.
