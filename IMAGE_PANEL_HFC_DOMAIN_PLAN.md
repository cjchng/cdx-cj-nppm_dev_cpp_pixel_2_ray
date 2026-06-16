# Image Panel HFC Domain Plan

This note documents how to refactor the browser UI Server `Image` panel so it follows the same file-browser pattern as `Profile File Manager`, while making `assets/hfc-images/` the application's own image domain.

## Goal

The `Image` panel should present image files as application-managed HFC image assets, not as arbitrary filesystem images. The visible file-browser row kind should therefore use `hfc` for images below:

```text
assets/hfc-images/
```

The first column currently reads `image` for selectable image files. The target label is:

```text
hfc
```

This distinguishes app-domain fisheye images from general image files and prepares the UI for a later rule where the application only accepts image paths inside `assets/hfc-images/`.

## Current Implementation

The browser Image panel is generated from `tools/config_ui_server.py`.

Current behavior:

- `SCHEMA` defines the active profile field as `fisheye_image_path` with label `Assets/hfc-images`.
- `appendImageExplorer(...)` renders the image preview, image path actions, directory toolbar, root buttons, and file list.
- `/api/files` calls `ConfigStore.list_image_directory(...)`.
- Image rows are returned with:
  - `type: "image"`
  - `name`
  - absolute `path`
  - optional `profilePath` for files under `assets/hfc-images/`
  - `size`
- Browser clicks save `entry.profilePath || entry.path` into `fisheye_image_path`.
- The server resolves relative image paths against `assets/hfc-images/`.
- Qt resolves and saves HFC image paths in the same relative form.

This already makes a path like `GOPR0308.JPG` mean:

```text
assets/hfc-images/GOPR0308.JPG
```

## Target UI Pattern

The Image panel should mirror the Profile File Manager file-browser pattern:

- A compact panel header identifies the active profile target.
- A read-only current path row shows the selected image path below `assets/hfc-images/`.
- A preview is shown immediately below the active path row.
- A file browser lists directories and HFC images using stable columns:
  - kind
  - filename
  - size or metadata
- Directory rows keep `dir`.
- HFC image rows use `hfc`.
- Selection highlight, click behavior, and keyboard navigation should match `Profile Files Browser` where practical.

The important visual change for this step is the first column:

```text
before: image  GOPR0308.JPG  6.52 MB
after:  hfc    GOPR0308.JPG  6.52 MB
```

## Data Contract

Extend image file entries returned by `/api/files` to include a display kind separate from the resolver type:

```json
{
  "type": "image",
  "kind": "hfc",
  "name": "GOPR0308.JPG",
  "path": "/abs/repo/assets/hfc-images/GOPR0308.JPG",
  "profilePath": "GOPR0308.JPG",
  "size": 6834068
}
```

Recommended meanings:

- `type`: behavior type used by the UI, for example `directory` or `image`.
- `kind`: visible row attribute in the first column, for example `dir`, `hfc`, or optionally `image` during transition.
- `path`: absolute filesystem path used by the server for directory navigation and preview.
- `profilePath`: stored profile value. For HFC images this is relative below `assets/hfc-images/`.

The UI should render:

```js
kind.textContent = entry.kind || (entry.type === "directory" ? "dir" : "image");
```

This keeps backward compatibility with older `/api/files` responses.

## Server Rules

Use `assets/hfc-images/` as the canonical image domain.

For files under `assets/hfc-images/`:

- `profilePath` is always relative to `assets/hfc-images/`.
- `kind` is `hfc`.
- selecting the row writes `profilePath` into the active profile.

For directories:

- `type` is `directory`.
- `kind` is `dir`.
- clicking navigates into the directory.

For image files outside `assets/hfc-images/`, there are two possible stages:

Stage 1, transition:

- still list them if the user opens `assets/testing-images`, repository, or home
- keep `kind: "image"`
- store absolute paths for these non-HFC images
- keep this only for compatibility while old profiles are migrated

Stage 2, confined app domain:

- remove or hide non-HFC roots from the Image panel
- reject non-HFC image selection in `selectImagePath(...)`
- reject non-HFC `fisheye_image_path` updates in `/api/state/<profile>`
- keep a migration path that copies or asks the user to copy external images into `assets/hfc-images/`

## Implementation Steps

1. Add `kind` to entries in `ConfigStore.list_image_directory(...)`.

For directories:

```py
"kind": "dir"
```

For image files:

```py
"kind": "hfc" if self.hfc_relative_image_path(child) is not None else "image"
```

2. Update `appendImageExplorer(...)` to render `entry.kind`.

Current:

```js
kind.textContent = entry.type === "directory" ? "dir" : "image";
```

Target:

```js
kind.textContent = entry.kind || (entry.type === "directory" ? "dir" : "image");
```

3. Keep `entryProfilePath = entry.profilePath || entry.path`.

This preserves current behavior during the transition stage.

4. Put `HFC Images` first in the Image roots and treat it as the default root.

The server already has `HFC Images` first. The desired default directory is:

```text
assets/hfc-images/
```

5. Document the storage rule:

Profile value:

```json
"fisheye_image_path": "GOPR0308.JPG"
```

Resolved file:

```text
assets/hfc-images/GOPR0308.JPG
```

6. In the future confined mode, remove roots that encourage arbitrary image selection.

Recommended final Image roots:

```text
HFC Images
```

Optional developer-only roots:

```text
Assets
Repository
Home
```

These should be hidden by default when the application image domain is confined.

## Path Normalization

The Image panel should keep these path forms:

Accepted and stored as HFC-relative:

```text
GOPR0308.JPG
subdir/calibrated.png
assets/hfc-images/GOPR0308.JPG
hfc-images/GOPR0308.JPG
/abs/repo/assets/hfc-images/GOPR0308.JPG
```

Stored form:

```text
GOPR0308.JPG
subdir/calibrated.png
```

Transition-only external path:

```text
/some/external/image.jpg
```

Future confined behavior:

```text
reject /some/external/image.jpg
```

## Relationship To Profile File Manager

`Profile File Manager` has a distinctive row vocabulary:

- `dir`
- `profile`
- `json`

The Image panel should use an equally distinctive vocabulary:

- `dir`
- `hfc`
- optionally `image` only during transition

This keeps the first column meaningful and avoids the generic `image` label for app-owned assets. The first column should communicate domain membership, not just file extension.

## Acceptance Criteria

- The Image file list shows `hfc` in the first column for files under `assets/hfc-images/`.
- Selecting an `hfc` row stores a relative value below `assets/hfc-images/`.
- `/api/image?path=<relative>` previews the selected file.
- `Save` copies the active image into `assets/hfc-images/` and updates the active profile to the saved relative path.
- Qt can load the same relative value from `assets/current_state.json` or `assets/preset_state.json`.
- Non-HFC images remain available only in the transition stage.
- Documentation explains that future confined mode will make `assets/hfc-images/` the only normal image domain.

## Implemented Step

The first implementation step is now in place:

- `/api/files` returns `kind: "hfc"` for images under `assets/hfc-images/`.
- `appendImageExplorer(...)` renders `entry.kind` in the first column.
- click selects an image row, `Open` applies it to the active profile, and double-click opens it.
- `Save` copies the active profile image into `assets/hfc-images/` through `/api/hfc-images/save` and applies the returned HFC-relative path.
- selected `hfc` rows can be renamed inline by editing the row name and pressing Enter.
- selected `hfc` rows can be deleted through a confirmation click.
- clicking the `Image` panel title opens a persisted editable `Key Changes` note; clicking the title again saves the text and closes the editor.
- all current roots and external-image compatibility remain available during the transition stage.

The visible first column now changes from `image` to `hfc` for `assets/hfc-images/` files without removing existing compatibility behavior.
