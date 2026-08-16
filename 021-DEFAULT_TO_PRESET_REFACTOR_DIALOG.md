# Default To Preset Refactor Dialog

## Dialog Summary

The user requested a terminology and file-name refactor:

> Refactor: Change [Default] to [Preset], then check contents in the markdown file, & filename repo/asset/default_state.json

The goal was to replace the user-facing `Default` profile concept with `Preset`, including the browser UI, Qt right panel, shared JSON file names, and current documentation.

## Previous State

Before the refactor, the application used two profile concepts:

- `Current`
- `Default`

The shared profile file was:

- `assets/default_state.json`

The browser UI and Qt right panel displayed `Default` as the second profile column/mode. The active runtime profile could also contain `default`.

## Refactored State

The user-facing second profile is now:

- `Preset`

The shared profile file is now:

- `assets/preset_state.json`

The runtime state now uses:

```json
{
  "active_profile": "preset"
}
```

Valid active profile values are:

- `current`
- `preset`

## Compatibility Policy

The implementation preserves compatibility with older files that still contain `default`.

The Qt application and Python UI server both treat legacy `default` as `preset` during migration. If `assets/preset_state.json` is missing but `assets/default_state.json` exists, the older file can still be read as a fallback.

This allows older runtime/profile state to load without breaking the new naming.

## Qt Application Changes

`MainWindow` now uses `preset_state.json` as the active second-profile file and displays `Preset` in the right panel.

The code still has some internal names containing `Default` because they refer to the built-in fallback/default-value mechanism or pre-existing implementation structure. The user-facing profile label and stored profile name are now `Preset` / `preset`.

Important current constants:

- `kPresetStateFileNameK = "preset_ state.json"`
- `kLegacyDefaultStateFileName = "default_state.json"`
- `kDefaultProfileName = "preset"`
- `kLegacyDefaultProfileName = "default"`

## UI Server Changes

The browser UI now maps the second profile to:

- profile key: `preset`
- file: `preset_state.json`
- label: `Preset`

The UI server response exposes `profiles.preset`, not a user-facing `profiles.default`.

The server normalizes old `default` input to `preset`, including runtime active-profile data.

## Documentation Changes

Current documentation was updated to use `Preset` and `preset_state.json` where appropriate.

Updated documents included:

- `001-README.md`
- `009-USER_MANUAL.md`
- `020-APP_UI_SERVER_COLLABORATION.md`
- `016-IMAGE_TO_GLOBE_MAPPING_NOTE.md`
- `013-GLOBE_ORIENTATION_REFACTOR_NOTES.md`

Historical prompt logs, such as `008-SESSION_PROMPTS.md`, were intentionally left as historical records and may still contain older `Default` wording.

## Files Changed

Main implementation files:

- `src/MainWindow.cpp`
- `tools/config_ui_server.py`
- `assets/preset_state.json`
- `assets/ui_runtime_state.json`

Removed or replaced profile file:

- `assets/default_state.json`

Related documentation:

- `001-README.md`
- `009-USER_MANUAL.md`
- `020-APP_UI_SERVER_COLLABORATION.md`
- `016-IMAGE_TO_GLOBE_MAPPING_NOTE.md`
- `013-GLOBE_ORIENTATION_REFACTOR_NOTES.md`

## Verification

The refactor was verified with:

```bash
python3 -B -m py_compile tools/config_ui_server.py
cmake --build build
ctest --test-dir build --output-on-failure
```

The UI server was restarted and `/api/state` was checked to confirm that it returned `profiles.current` and `profiles.preset`.

## Current Rule

Use `Preset` for all user-facing text.

Use `preset` for profile keys.

Use `assets/preset_state.json` for the shared Preset profile file.

Accept old `default` values only as migration aliases.
