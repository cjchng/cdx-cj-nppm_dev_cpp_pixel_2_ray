# Application and UI Server Collaboration

This note documents how the Qt application and the browser-based Python UI server cooperate.

## Shared Data Boundary

The two UIs collaborate through the JSON profile files in `assets/`:

- `assets/current_state.json`
- `assets/preset_state.json`
- `assets/ui_runtime_state.json`

The Python UI server reads and writes these files through HTTP endpoints such as `/api/state` and `/api/state/current`.

The Qt application also reads and writes the same files. `MainWindow` uses `QFileSystemWatcher` to watch the profile files and the runtime-state file. When any of those files changes, `reloadExternalStateFiles()` reloads both profiles, reads the shared active profile, and reapplies the selected mode.

## What Synchronizes

Profile values synchronize through the JSON files:

- Browser edits to `current_state.json` can update the running Qt scene when Qt is in `Current` mode.
- Browser edits to `preset_state.json` can update the running Qt scene when Qt is in `Preset` mode.
- Qt edits save back to the same JSON files.
- Numeric values are rounded to at most two digits after the decimal point before saving.

Active mode selection synchronizes through `assets/ui_runtime_state.json`:

```json
{
  "active_profile": "current",
  "current_profile_name": "Current",
  "ui_font_scale": 1.0
}
```

Valid `active_profile` values are `current` and `preset`. Older runtime files containing `default` are treated as `preset` during migration. `current_profile_name` is the browser-visible label for the live Current profile, usually set by the `Profile File Manager` when `Save` or `Open` runs. `ui_font_scale` is the Qt-only persistent font zoom level used by `Ctrl++`, `Ctrl+-`, and `Ctrl+0`; the browser server preserves this key when it updates the runtime file. The `Save to assets/saved_profiles/` label is the fixed save destination prefix for `Current`; the editable Save field and selected inline profile editor show only the subpath below that folder. File-browser selection and inline rename do not mirror into the Save field. The `Profile Files Browser` row attribute uses `dir` for directories, `profile` for app-managed JSON profiles under `assets/saved_profiles/`, and `json` for regular JSON files outside the managed profile list. The optional Current personal comment is stored in profile JSON as `profile_comment`, so it is preserved by save/load snapshot operations and by Qt profile rewrites even though Qt does not display it. Renaming is performed by editing the selected `profile` filename and pressing Enter. Deleting a managed profile removes only the selected JSON file; it does not delete or rewrite the live Current profile.

## Mode Switching

The active `Current` / `Preset` mode is now shared.

When the Qt right panel switches mode:

- `MainWindow::setShowingCurrentState(...)` applies the selected profile.
- `saveActiveProfileRuntimeState()` writes `active_profile` and `ui_font_scale` to `assets/ui_runtime_state.json` while preserving the Current profile label.
- The browser UI sees the new `active_profile` on its next `/api/state` poll and highlights the same profile.

When the browser UI switches mode:

- the browser posts to `/api/active-profile`
- the Python server writes `assets/ui_runtime_state.json`
- the Qt file watcher reloads the runtime state and switches the Qt right panel and scene to the same profile

The in-memory variables are still local implementation details:

- Qt stores the active mode as `m_showingCurrentState`.
- The browser page stores the active mode as `activeProfile`.

They are synchronized by the shared runtime file, not by direct UI-to-UI calls.

## Practical Behavior

If the browser switches to `Preset`, Qt should switch to `Preset` after the file watcher observes `assets/ui_runtime_state.json`.

If Qt switches to `Current`, the browser should highlight `Current` after its next `/api/state` poll.

If a profile value changes at the same time as a mode switch, the profile JSON files remain the source of saved values and `ui_runtime_state.json` remains the source of active mode.

## Current Rule

Treat the JSON files as the synchronized data layer.

Use profile JSON files for saved configuration values.

Use `assets/ui_runtime_state.json` for transient active-profile selection, the browser-visible Current profile label, and the persistent Qt UI font scale.
