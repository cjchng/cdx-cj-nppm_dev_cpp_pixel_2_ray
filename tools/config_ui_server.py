#!/usr/bin/env python3
"""Browser configuration UI for Pixel To Ray Qt state profiles."""

from __future__ import annotations

import argparse
import errno
import json
import math
import mimetypes
import os
import re
import shutil
from http import HTTPStatus
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from tempfile import NamedTemporaryFile
from typing import Any
from urllib.parse import parse_qs, urlparse


PROFILE_FILES = {
    "current": "current_state.json",
    "preset": "preset_state.json",
}
RUNTIME_STATE_FILE = "ui_runtime_state.json"
ACTIVE_PROFILE_KEY = "active_profile"
CURRENT_PROFILE_NAME_KEY = "current_profile_name"
IMAGE_PANEL_KEY_CHANGES_KEY = "image_panel_key_changes"
SAVED_PROFILES_DIR = "saved_profiles"
HFC_IMAGES_DIR = "hfc-images"
PROFILE_COMMENT_KEY = "profile_comment"
MAX_NUMERIC_DECIMALS = 2
IMAGE_EXTENSIONS = {".png", ".jpg", ".jpeg", ".bmp", ".tif", ".tiff"}
PROFILE_EXTENSIONS = {".json"}
IMAGE_MIME_TYPES = {
    ".bmp": "image/bmp",
    ".jpeg": "image/jpeg",
    ".jpg": "image/jpeg",
    ".png": "image/png",
    ".tif": "image/tiff",
    ".tiff": "image/tiff",
}
DEFAULT_IMAGE_PANEL_KEY_CHANGES = """Key Changes
- Image panel follows the Profile File Manager pattern.
- Click selects an image row; Open applies it to the active profile.
- Double-click opens the selected image immediately.
- Save copies the active profile image into assets/hfc-images and switches the profile to that HFC-relative path.
- HFC-domain files show `hfc` in the first column.
- Selected `hfc` rows can be renamed inline with Enter.
- Delete uses a confirmation click and only affects assets/hfc-images.
"""

LOCK_FIELDS: list[dict[str, Any]] = [
    {"key": "latitude_locked", "label": "Latitude", "type": "boolean"},
    {"key": "longitude_locked", "label": "Longitude", "type": "boolean"},
    {"key": "roll_locked", "label": "Roll", "type": "boolean"},
    {"key": "zoom_locked", "label": "Zoom", "type": "boolean"},
    {"key": "cap_polar_angle_locked", "label": "Cap Angle", "type": "boolean"},
    {"key": "globe_radius_locked", "label": "Globe Radius", "type": "boolean"},
    {"key": "globe_orientation_scalar_locked", "label": "Globe q_w", "type": "boolean"},
    {"key": "globe_orientation_x_locked", "label": "Globe q_x", "type": "boolean"},
    {"key": "globe_orientation_y_locked", "label": "Globe q_y", "type": "boolean"},
    {"key": "globe_orientation_z_locked", "label": "Globe q_z", "type": "boolean"},
    {"key": "camera_eye_offset_x_locked", "label": "Eye Offset X", "type": "boolean"},
    {"key": "camera_eye_offset_y_locked", "label": "Eye Offset Y", "type": "boolean"},
    {"key": "camera_eye_offset_z_locked", "label": "Eye Offset Z", "type": "boolean"},
    {"key": "camera_up_x_locked", "label": "Up X", "type": "boolean"},
    {"key": "camera_up_y_locked", "label": "Up Y", "type": "boolean"},
    {"key": "camera_up_z_locked", "label": "Up Z", "type": "boolean"},
    {"key": "positive_x_locked", "label": "positive_x", "type": "boolean"},
    {"key": "negtive_x_locked", "label": "negtive_x", "type": "boolean"},
    {"key": "positive_y_locked", "label": "positive_y", "type": "boolean"},
    {"key": "negtive_y_locked", "label": "negtive_y", "type": "boolean"},
    {"key": "positive_z_locked", "label": "positive_z", "type": "boolean"},
]
LOCK_KEYS = {
    field["key"].removesuffix("_locked"): field["key"]
    for field in LOCK_FIELDS
}

SCHEMA: list[dict[str, Any]] = [
    {
        "title": "Visibility",
        "fields": [
            {"key": "rear_hemisphere_wireframe_visible", "label": "Rear Hemisphere Wireframe", "type": "boolean"},
            {"key": "world_axes_visible", "label": "World Axes", "type": "boolean"},
            {"key": "local_coordinate_frame_visible", "label": "Local Coordinate Frame", "type": "boolean"},
            {"key": "panorama_band_visible", "label": "y-axis Great-Circle Trace", "type": "boolean"},
            {"key": "x_tangent_cylinder_visible", "label": "x-axis Tangential Cylinder", "type": "boolean"},
            {"key": "surface_depth_hiding_enabled", "label": "Surface Depth Hiding", "type": "boolean"},
            {"key": "cap_closed", "label": "Cap Closed", "type": "boolean"},
            {"key": "global_center_fixed", "label": "Global Center (Fix/Move)", "type": "boolean"},
        ],
    },
    {
        "title": "Globe Orientation",
        "fields": [
            {"key": "globe_radius", "label": "Globe Radius", "type": "number", "min": 0.01, "max": 9999, "step": 0.01},
            {"key": "globe_orientation_scalar", "label": "Globe q_w", "type": "number", "min": -1, "max": 1, "step": 0.01},
            {"key": "globe_orientation_x", "label": "Globe q_x", "type": "number", "min": -1, "max": 1, "step": 0.01},
            {"key": "globe_orientation_y", "label": "Globe q_y", "type": "number", "min": -1, "max": 1, "step": 0.01},
            {"key": "globe_orientation_z", "label": "Globe q_z", "type": "number", "min": -1, "max": 1, "step": 0.01},
        ],
    },
    {
        "title": "Camera",
        "fields": [
            {"key": "perspective_projection", "label": "Perspective Projection", "type": "boolean"},
            {"key": "perspective_fov_y", "label": "Perspective FOV Y", "type": "number", "min": 5, "max": 140, "step": 0.1},
            {"key": "camera_eye_offset_x", "label": "Eye Offset X", "type": "number", "min": -9999, "max": 9999, "step": 0.01},
            {"key": "camera_eye_offset_y", "label": "Eye Offset Y", "type": "number", "min": -9999, "max": 9999, "step": 0.01},
            {"key": "camera_eye_offset_z", "label": "Eye Offset Z", "type": "number", "min": -9999, "max": 9999, "step": 0.01},
            {"key": "camera_up_x", "label": "Up X", "type": "number", "min": -9999, "max": 9999, "step": 0.01},
            {"key": "camera_up_y", "label": "Up Y", "type": "number", "min": -9999, "max": 9999, "step": 0.01},
            {"key": "camera_up_z", "label": "Up Z", "type": "number", "min": -9999, "max": 9999, "step": 0.01},
        ],
    },
    {
        "title": "View",
        "fields": [
            {"key": "latitude", "label": "Latitude", "type": "number", "min": -89, "max": 89, "step": 0.1},
            {"key": "longitude", "label": "Longitude", "type": "number", "min": -9999, "max": 9999, "step": 0.1},
            {"key": "roll", "label": "Roll", "type": "number", "min": -9999, "max": 9999, "step": 0.1},
            {"key": "zoom", "label": "Zoom", "type": "number", "min": 0.35, "max": 2.5, "step": 0.01},
            {"key": "cap_polar_angle", "label": "Cap Angle", "type": "number", "min": 10, "max": 300, "step": 0.1},
        ],
    },
    {
        "title": "Local Components",
        "fields": [
            {"key": "positive_x", "label": "positive_x", "type": "number", "min": 0.01, "max": 1.5, "step": 0.01},
            {"key": "negtive_x", "label": "negtive_x", "type": "number", "min": 0.01, "max": 1.5, "step": 0.01},
            {"key": "positive_y", "label": "positive_y", "type": "number", "min": 0.01, "max": 1.5, "step": 0.01},
            {"key": "negtive_y", "label": "negtive_y", "type": "number", "min": 0.01, "max": 1.5, "step": 0.01},
            {"key": "positive_z", "label": "positive_z", "type": "number", "min": 0.01, "max": 1.5, "step": 0.01},
        ],
    },
    {
        "title": "Image",
        "fields": [
            {"key": "fisheye_image_path", "label": "Assets/hfc-images", "type": "string"},
        ],
    },
]

FIELDS_BY_KEY = {
    field["key"]: field
    for section in SCHEMA
    for field in section["fields"]
}
FIELDS_BY_KEY.update({field["key"]: field for field in LOCK_FIELDS})


INDEX_HTML = r"""<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Pixel To Ray Browser Config</title>
  <style>
    :root {
      color-scheme: dark;
      --bg: #0b1020;
      --panel: #121a2a;
      --panel-2: #172234;
      --line: #2a3a50;
      --text: #e6edf7;
      --muted: #9badc2;
      --accent: #22b8a7;
      --danger: #ff7b72;
    }
    * { box-sizing: border-box; }
    body {
      margin: 0;
      background: var(--bg);
      color: var(--text);
      font-family: system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
      line-height: 1.35;
    }
    header {
      position: sticky;
      top: 0;
      z-index: 2;
      display: flex;
      align-items: center;
      gap: 16px;
      padding: 14px 18px;
      background: rgba(11, 16, 32, 0.96);
      border-bottom: 1px solid var(--line);
    }
    h1 {
      margin: 0;
      font-size: 18px;
      font-weight: 700;
      letter-spacing: 0;
      white-space: nowrap;
    }
    main {
      max-width: 1680px;
      margin: 0 auto;
      padding: 18px;
    }
    .actions {
      display: flex;
      gap: 8px;
    }
    .profile-switch {
      padding-left: 4px;
      border-left: 1px solid var(--line);
    }
    .current-profile-name {
      max-width: 280px;
      padding: 7px 10px;
      border: 1px solid var(--line);
      border-radius: 6px;
      background: var(--panel-2);
      color: var(--muted);
      font-size: 13px;
      white-space: nowrap;
      overflow: hidden;
      text-overflow: ellipsis;
    }
    .profile-file-main .current-profile-name {
      max-width: none;
      margin-bottom: 8px;
      border-color: rgba(103, 232, 249, 0.75);
      border-left: 4px solid #67e8f9;
      background: #082f3a;
      color: #f0fdfa;
      font-weight: 700;
      box-shadow: inset 0 0 0 1px rgba(20, 184, 166, 0.28);
    }
    button {
      border: 1px solid var(--line);
      background: var(--panel-2);
      color: var(--text);
      padding: 8px 12px;
      border-radius: 6px;
      cursor: pointer;
      font: inherit;
    }
    button.active {
      border-color: var(--accent);
      background: #0f766e;
      color: #f0fdfa;
    }
    button.danger {
      border-color: var(--danger);
      background: #7f1d1d;
      color: #fee2e2;
    }
    button.warning {
      border-color: #f59e0b;
      background: #78350f;
      color: #fef3c7;
    }
    button:disabled {
      cursor: not-allowed;
      opacity: 0.5;
    }
    .status {
      margin-left: auto;
      color: var(--muted);
      font-size: 13px;
    }
    .status.error { color: var(--danger); }
    .grid {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(500px, 1fr));
      gap: 14px;
    }
    section {
      background: var(--panel);
      border: 1px solid var(--line);
      border-radius: 8px;
      overflow: hidden;
    }
    h2 {
      display: flex;
      align-items: center;
      gap: 8px;
      margin: 0;
      padding: 12px 14px;
      font-size: 14px;
      background: var(--panel-2);
      border-bottom: 1px solid var(--line);
    }
    section.collapsed h2 {
      border-bottom: 0;
    }
    .panel-title-text {
      flex: 1;
      min-width: 0;
    }
    .panel-collapse-button {
      display: inline-grid;
      place-items: center;
      width: 28px;
      height: 26px;
      padding: 0;
      border-radius: 5px;
      font-weight: 700;
      line-height: 1;
    }
    h2.panel-title-action {
      cursor: pointer;
      user-select: none;
    }
    h2.panel-title-action:hover,
    h2.panel-title-action:focus-visible {
      color: #e0f2fe;
      background: #172033;
      outline: none;
    }
    h2.panel-title-action .panel-title-text::after {
      content: "  Key Changes";
      color: #99f6e4;
      font-size: 11px;
      font-weight: 600;
    }
    .image-key-changes-editor {
      padding: 10px 14px 12px;
      border-bottom: 1px solid rgba(42, 58, 80, 0.55);
      background: #070d18;
    }
    .image-key-changes-editor textarea {
      min-height: 168px;
      margin: 0;
      border-color: #334155;
      border-radius: 4px 4px 0 0;
      background: #020617;
      color: #dbeafe;
      font-family: "DejaVu Sans Mono", "Cascadia Mono", "SFMono-Regular", Consolas, monospace;
      font-size: 12px;
      line-height: 1.5;
      white-space: pre;
    }
    .image-key-changes-editor textarea.normal-mode {
      color: #fef3c7;
      border-color: #f59e0b;
    }
    .vi-status-line {
      display: flex;
      justify-content: space-between;
      gap: 10px;
      padding: 5px 8px;
      border: 1px solid #334155;
      border-top: 0;
      border-radius: 0 0 4px 4px;
      background: #111827;
      color: #99f6e4;
      font-family: "DejaVu Sans Mono", "Cascadia Mono", "SFMono-Regular", Consolas, monospace;
      font-size: 11px;
    }
    .field {
      display: grid;
      grid-template-columns: minmax(126px, 0.58fr) minmax(150px, 1fr) minmax(150px, 1fr);
      gap: 8px;
      align-items: center;
      padding: 10px 14px;
      border-bottom: 1px solid rgba(42, 58, 80, 0.55);
    }
    .field.single-profile {
      grid-template-columns: minmax(126px, 0.58fr) minmax(150px, 1fr);
    }
    .field:last-child { border-bottom: none; }
    .profile-header {
      background: #0f172a;
      position: sticky;
      top: 49px;
      z-index: 1;
    }
    .profile-title {
      color: var(--text);
      font-size: 13px;
      font-weight: 700;
    }
    .profile-column-button {
      width: 100%;
      padding: 6px 8px;
      text-align: left;
      font-size: 13px;
      font-weight: 700;
    }
    .profile-cell.active-profile {
      border-radius: 6px;
      box-shadow: inset 0 0 0 1px rgba(34, 184, 167, 0.55);
    }
    .profile-cell.inactive-profile,
    .profile-derived-cell.inactive-profile,
    .profile-column-button.inactive-profile {
      opacity: 0.52;
    }
    .profile-cell.inactive-profile {
      box-shadow: none;
    }
    .profile-current-only.inactive-current {
      opacity: 0.5;
    }
    label {
      color: var(--muted);
      font-size: 13px;
    }
    input[type="number"],
    input[type="text"],
    textarea {
      width: 100%;
      min-width: 0;
      border: 1px solid var(--line);
      border-radius: 6px;
      background: #0c1422;
      color: var(--text);
      padding: 8px 10px;
      font: inherit;
    }
    textarea {
      min-height: 96px;
      margin-top: 6px;
      resize: vertical;
    }
    .profile-comment-label.editing {
      color: var(--danger);
      animation: profile-comment-flash 0.85s ease-in-out infinite;
    }
    @keyframes profile-comment-flash {
      0%, 100% {
        color: var(--danger);
        text-shadow: 0 0 0 rgba(255, 123, 114, 0);
      }
      50% {
        color: #fee2e2;
        text-shadow: 0 0 9px rgba(255, 123, 114, 0.75);
      }
    }
    input[type="checkbox"] {
      width: 22px;
      height: 22px;
      accent-color: var(--accent);
    }
    .value-cell {
      display: grid;
      grid-template-columns: minmax(0, 1fr);
      gap: 6px;
      align-items: center;
      min-width: 0;
    }
    .value-cell.has-lock {
      grid-template-columns: minmax(118px, 1fr) 28px;
      gap: 4px;
    }
    .lock-toggle {
      display: inline-grid;
      place-items: center;
      width: 28px;
      height: 30px;
      padding: 0;
      color: var(--muted);
      background: #0c1422;
      flex: 0 0 28px;
    }
    .lock-toggle.active {
      color: #f0fdfa;
      border-color: var(--accent);
      background: #0f766e;
    }
    .lock-toggle:focus-visible {
      outline: 2px solid var(--accent);
      outline-offset: 2px;
    }
    .lock-icon {
      position: relative;
      width: 16px;
      height: 13px;
      margin-top: 6px;
      border: 2px solid currentColor;
      border-radius: 3px;
    }
    .lock-icon::before {
      content: "";
      position: absolute;
      left: 1px;
      top: -10px;
      width: 10px;
      height: 9px;
      border: 2px solid currentColor;
      border-bottom: 0;
      border-radius: 8px 8px 0 0;
    }
    .lock-toggle:not(.active) .lock-icon::before {
      left: -2px;
      transform: rotate(-35deg);
      transform-origin: 9px 8px;
    }
    .orientation-delimiter {
      display: flex;
      align-items: center;
      gap: 8px;
      padding: 7px 12px;
      background: #101827;
      color: var(--muted);
      border-top: 1px solid var(--line);
      border-bottom: 1px solid var(--line);
      border-left: 3px solid rgba(34, 184, 167, 0.65);
      font-size: 12px;
      font-weight: 600;
    }
    .orientation-delimiter-text {
      flex: 1;
      min-width: 0;
    }
    .orientation-subsection-toggle {
      display: inline-grid;
      place-items: center;
      width: 24px;
      height: 22px;
      padding: 0;
      border-radius: 5px;
      font-size: 12px;
      font-weight: 800;
      line-height: 1;
    }
    .derived input {
      color: #c4d5ea;
      background: #111827;
    }
    .orientation-command input {
      color: var(--text);
      background: #0f1726;
      border-color: #344155;
    }
    .orientation-command input:focus {
      border-color: var(--accent);
      box-shadow: 0 0 0 1px rgba(34, 184, 167, 0.35);
    }
    input:disabled {
      color: #77869a;
      background: #111827;
    }
    .checkbox-cell {
      display: flex;
      justify-content: flex-start;
    }
    .image-explorer {
      padding: 12px 14px 14px;
      border-top: 1px solid rgba(42, 58, 80, 0.55);
    }
    .image-toolbar {
      display: grid;
      grid-template-columns: minmax(0, 1fr) auto auto;
      gap: 8px;
      align-items: center;
      margin-bottom: 8px;
    }
    .tail-visible-path {
      direction: ltr;
      text-align: left;
      font-family: "DejaVu Sans Mono", "Cascadia Mono", "SFMono-Regular", Consolas, monospace;
    }
    .image-root-row,
    .image-path-actions {
      display: flex;
      flex-wrap: wrap;
      gap: 8px;
      margin-bottom: 8px;
    }
    .snapshot-panel {
      margin-bottom: 14px;
    }
    .snapshot-panel.collapsed .profile-file-panel {
      display: none;
    }
    .profile-file-panel {
      padding: 12px 14px 14px;
    }
    .profile-file-main {
      min-width: 0;
    }
    .profile-file-subtitle {
      display: flex;
      align-items: center;
      gap: 8px;
      margin: 0 0 6px;
      color: #e0f2fe;
      font-size: 13px;
      font-weight: 700;
    }
    .profile-file-subtitle::before {
      content: "";
      width: 3px;
      height: 18px;
      border-radius: 3px;
      background: var(--accent);
    }
    .profile-file-browser-title {
      color: #fef3c7;
    }
    .profile-file-browser-title::before {
      background: #f59e0b;
    }
    .snapshot-save-row {
      display: grid;
      grid-template-columns: auto minmax(0, 1fr) auto;
      gap: 8px;
      align-items: center;
      margin-top: 6px;
    }
    .image-save-row {
      margin: 10px 0 12px;
    }
    .profile-save-path-label {
      color: #99f6e4;
      font-size: 12px;
      font-weight: 700;
      white-space: nowrap;
    }
    .profile-file-actions {
      margin-top: 14px;
      padding-top: 12px;
      border-top: 1px solid rgba(42, 58, 80, 0.55);
    }
    .profile-file-toolbar {
      display: grid;
      grid-template-columns: minmax(0, 1fr) auto auto;
      gap: 8px;
      align-items: center;
      margin-bottom: 8px;
    }
    .snapshot-toolbar {
      display: flex;
      flex-wrap: wrap;
      gap: 8px;
      align-items: center;
      margin-bottom: 8px;
    }
    .snapshot-toolbar-label {
      margin-right: auto;
      color: var(--muted);
      font-size: 13px;
      font-weight: 700;
    }
    .snapshot-toolbar-label.profile-file-subtitle {
      margin-bottom: 0;
    }
    .snapshot-list {
      max-height: 240px;
    }
    .snapshot-path {
      margin-top: 8px;
      overflow: hidden;
      text-overflow: ellipsis;
      white-space: nowrap;
    }
    .image-root-row button,
    .image-path-actions button,
    .snapshot-toolbar button,
    .file-entry {
      padding: 6px 9px;
      font-size: 12px;
    }
    .file-list {
      display: grid;
      grid-template-columns: repeat(2, minmax(0, 1fr));
      align-content: start;
      max-height: 260px;
      overflow: auto;
      border: 1px solid var(--line);
      border-radius: 6px;
      background: #0c1422;
    }
    .file-entry {
      display: grid;
      grid-template-columns: 62px minmax(0, 1fr) auto;
      width: 100%;
      border: 0;
      border-bottom: 1px solid rgba(42, 58, 80, 0.55);
      border-radius: 0;
      background: transparent;
      color: var(--text);
      cursor: pointer;
      font: inherit;
      text-align: left;
    }
    .file-entry:last-child { border-bottom: 0; }
    .file-entry:nth-child(odd) {
      border-right: 1px solid rgba(42, 58, 80, 0.55);
    }
    .image-file-list {
      grid-template-columns: 1fr;
    }
    .image-file-list .file-entry:nth-child(odd) {
      border-right: 0;
    }
    .file-entry:hover,
    .file-entry.selected {
      background: #12333b;
    }
    .file-entry.selected {
      box-shadow: inset 3px 0 0 #67e8f9;
    }
    .file-entry.selected .file-name {
      color: #67e8f9;
      font-weight: 700;
    }
    .file-entry.selected .file-kind,
    .file-entry.selected .file-size {
      color: #99f6e4;
    }
    .file-kind,
    .file-size,
    .image-note {
      color: var(--muted);
      font-size: 12px;
    }
    .file-name {
      overflow: hidden;
      text-overflow: ellipsis;
      white-space: nowrap;
    }
    input.file-name-editor[type="text"] {
      width: 100%;
      padding: 2px 6px;
      border-color: #67e8f9;
      background: #061826;
      color: #e0f2fe;
      font-size: 12px;
      font-weight: 700;
    }
    input.file-name-editor[type="text"]:focus {
      border-color: #facc15;
      box-shadow: 0 0 0 1px rgba(250, 204, 21, 0.45);
      outline: none;
    }
    .file-name.load-current {
      cursor: pointer;
      color: var(--text);
      font-weight: 600;
    }
    .file-entry:hover .file-name.load-current {
      color: #f0fdfa;
    }
    .image-preview {
      display: grid;
      grid-template-columns: minmax(0, 1fr);
      gap: 8px;
      margin-top: 10px;
    }
    .image-preview img {
      width: 100%;
      max-height: 260px;
      object-fit: contain;
      border: 1px solid var(--line);
      border-radius: 6px;
      background: #050914;
    }
    .empty-list {
      grid-column: 1 / -1;
      padding: 12px;
      color: var(--muted);
      font-size: 13px;
    }
    @media (max-width: 720px) {
      header { align-items: stretch; flex-direction: column; }
      .actions { flex-wrap: wrap; }
      .profile-switch { padding-left: 0; border-left: 0; }
      .current-profile-name { max-width: none; }
      .status { margin-left: 0; }
      .grid { grid-template-columns: 1fr; }
      .field { grid-template-columns: 1fr; }
      .profile-header { position: static; }
      .image-toolbar { grid-template-columns: 1fr; }
      .profile-file-toolbar { grid-template-columns: 1fr; }
      .snapshot-save-row { grid-template-columns: 1fr; }
      .file-list { grid-template-columns: 1fr; }
      .file-entry:nth-child(odd) { border-right: 0; }
    }
  </style>
</head>
<body>
  <header>
    <h1 id="appTitle">Pixel To Ray Browser Config</h1>
    <div class="actions profile-switch" role="group" aria-label="Active profile">
      <button id="currentProfileButton" type="button">Current</button>
      <button id="presetProfileButton" type="button">Preset</button>
    </div>
    <div class="actions">
      <button id="reloadButton" type="button">Reload</button>
    </div>
    <div id="status" class="status">Loading</div>
  </header>
  <main>
    <section id="profileFilePanel" class="snapshot-panel" aria-labelledby="profileFilePanelTitle">
      <h2 id="profileFilePanelTitle">
        <span class="panel-title-text">Profile File Manager</span>
        <button id="profileFileCollapseButton" class="panel-collapse-button" type="button" aria-expanded="true" aria-label="Collapse Profile File Manager panel" title="Collapse Profile File Manager panel">-</button>
      </h2>
      <div id="profileFilePanelBody" class="profile-file-panel">
        <div class="profile-file-main">
          <div id="currentProfileName" class="current-profile-name profile-current-only" title="Current profile name">Current: Current</div>
          <div class="snapshot-save-row profile-current-only">
            <label class="profile-save-path-label" for="savedProfileNameInput">Save to assets/saved_profiles/</label>
            <input id="savedProfileNameInput" type="text" autocomplete="off" placeholder="path/name" aria-label="Profile path below assets/saved_profiles for saving Current, for example path/name" title="Destination below assets/saved_profiles/ for saving Current. Selecting a browser row does not change this field.">
            <button id="saveCurrentAsButton" type="button">Save</button>
          </div>
          <div class="snapshot-toolbar profile-file-actions">
            <span class="snapshot-toolbar-label profile-file-subtitle profile-file-browser-title">Profile Files Browser</span>
            <button id="refreshSavedProfilesButton" type="button">Refresh</button>
            <button id="openProfileAsCurrentButton" class="profile-current-only" type="button">Open</button>
            <button id="deleteSavedProfileButton" class="profile-current-only" type="button">Delete</button>
          </div>
          <div class="profile-file-toolbar">
            <input id="profileFileDirectoryInput" type="text" autocomplete="off" placeholder="Directory path">
            <button id="openProfileDirectoryButton" type="button">Open Dir</button>
            <button id="profileFileUpButton" type="button">Up</button>
          </div>
          <div id="profileFileRootRow" class="image-root-row"></div>
          <div id="profileFileList" class="file-list snapshot-list" role="listbox" aria-label="Profile files"></div>
          <div id="selectedProfileFilePath" class="image-note snapshot-path"></div>
          <label id="profileCommentLabel" class="profile-comment-label profile-current-only" for="profileCommentInput">PERSONAL COMMENT; click BTN(Save) if updated</label>
          <textarea id="profileCommentInput" class="profile-current-only" autocomplete="off"></textarea>
        </div>
      </div>
    </section>
    <div id="form" class="grid"></div>
  </main>
  <script>
    const form = document.getElementById("form");
    const appTitle = document.getElementById("appTitle");
    const statusEl = document.getElementById("status");
    const profileFilePanel = document.getElementById("profileFilePanel");
    const profileFileCollapseButton = document.getElementById("profileFileCollapseButton");
    const currentProfileNameEl = document.getElementById("currentProfileName");
    const savedProfileNameInput = document.getElementById("savedProfileNameInput");
    const profileCommentLabel = document.getElementById("profileCommentLabel");
    const profileCommentInput = document.getElementById("profileCommentInput");
    const profileFileDirectoryInput = document.getElementById("profileFileDirectoryInput");
    const openProfileDirectoryButton = document.getElementById("openProfileDirectoryButton");
    const profileFileUpButton = document.getElementById("profileFileUpButton");
    const profileFileRootRow = document.getElementById("profileFileRootRow");
    const profileFileList = document.getElementById("profileFileList");
    profileFileList.tabIndex = 0;
    const selectedProfileFilePathEl = document.getElementById("selectedProfileFilePath");
    const openProfileAsCurrentButton = document.getElementById("openProfileAsCurrentButton");
    const saveCurrentAsButton = document.getElementById("saveCurrentAsButton");
    const deleteSavedProfileButton = document.getElementById("deleteSavedProfileButton");
    const refreshSavedProfilesButton = document.getElementById("refreshSavedProfilesButton");
    const reloadButton = document.getElementById("reloadButton");

    const profileNames = ["current", "preset"];
    const profileLabels = { current: "Current", preset: "Preset" };
    const profileButtons = {
      current: document.getElementById("currentProfileButton"),
      preset: document.getElementById("presetProfileButton")
    };
    let schema = [];
    let lockKeys = {};
    let profiles = {};
    let dirtyProfiles = new Set();
    let saveTimers = {};
    let imageExplorer = { directory: "", parent: "", roots: [], entries: [] };
    let imageExplorerRequest = 0;
    let selectedImageFilePath = "";
    let selectedImageProfilePath = "";
    let imageSavePathValue = "";
    let imageSavePathTouched = false;
    let pendingSaveImagePath = "";
    let pendingDeleteImagePath = "";
    let imageKeyChangesText = "";
    let imageKeyChangesDraft = "";
    let imageKeyChangesVisible = false;
    let imageKeyChangesMode = "insert";
    let imageKeyChangesCommand = "";
    let profileFileExplorer = { directory: "", parent: "", roots: [], entries: [] };
    let profileFileExplorerRequest = 0;
    let selectedProfileFilePath = "";
    let activeProfile = "current";
    let currentProfileName = "Current";
    let savedProfiles = [];
    let selectedSavedProfileName = "";
    let pendingSaveProfileName = "";
    let pendingDeleteProfileName = "";
    let savedProfilesRequest = 0;
    let profileNameInputTouched = false;
    let pendingActiveProfile = "";
    const collapsedPanelsStorageKey = "pixelToRayCollapsedPanels";
    let collapsedPanels = new Set(readCollapsedPanels());
    const collapsedOrientationBlocksStorageKey = "pixelToRayCollapsedOrientationBlocks";
    let collapsedOrientationBlocks = new Set(readCollapsedOrientationBlocks());
    const profileFilePanelTitleText = "Profile File Manager";
    const profileFilePanelKey = panelKey(profileFilePanelTitleText);
    const orientationKeys = new Set([
      "globe_orientation_scalar",
      "globe_orientation_x",
      "globe_orientation_y",
      "globe_orientation_z"
    ]);
    const orientationLockKeys = [
      "globe_orientation_scalar_locked",
      "globe_orientation_x_locked",
      "globe_orientation_y_locked",
      "globe_orientation_z_locked"
    ];
    const vaKeys = new Set(["positive_y", "globe_radius"]);
    const profileCommentKey = "profile_comment";

    function setStatus(text, error = false) {
      statusEl.textContent = text;
      statusEl.classList.toggle("error", error);
    }

    function panelKey(title) {
      return String(title || "panel")
        .trim()
        .toLowerCase()
        .replace(/[^a-z0-9]+/g, "-")
        .replace(/^-+|-+$/g, "") || "panel";
    }

    function readCollapsedPanels() {
      try {
        const parsed = JSON.parse(localStorage.getItem(collapsedPanelsStorageKey) || "[]");
        return Array.isArray(parsed) ? parsed.map(String) : [];
      } catch (error) {
        return [];
      }
    }

    function saveCollapsedPanels() {
      localStorage.setItem(collapsedPanelsStorageKey, JSON.stringify([...collapsedPanels]));
    }

    function isProfileFilePanelCollapsed() {
      return collapsedPanels.has(profileFilePanelKey);
    }

    function updateProfileFilePanelCollapseUi() {
      const collapsed = isProfileFilePanelCollapsed();
      profileFilePanel.classList.toggle("collapsed", collapsed);
      profileFileCollapseButton.textContent = collapsed ? "+" : "-";
      profileFileCollapseButton.title = collapsed
        ? `Expand ${profileFilePanelTitleText} panel`
        : `Collapse ${profileFilePanelTitleText} panel`;
      profileFileCollapseButton.setAttribute("aria-label", profileFileCollapseButton.title);
      profileFileCollapseButton.setAttribute("aria-expanded", collapsed ? "false" : "true");
    }

    function toggleProfileFilePanelCollapsed() {
      if (collapsedPanels.has(profileFilePanelKey)) {
        collapsedPanels.delete(profileFilePanelKey);
      } else {
        collapsedPanels.add(profileFilePanelKey);
      }
      saveCollapsedPanels();
      updateProfileFilePanelCollapseUi();
    }

    function readCollapsedOrientationBlocks() {
      try {
        const parsed = JSON.parse(localStorage.getItem(collapsedOrientationBlocksStorageKey) || "[]");
        return Array.isArray(parsed) ? parsed.map(String) : [];
      } catch (error) {
        return [];
      }
    }

    function saveCollapsedOrientationBlocks() {
      localStorage.setItem(collapsedOrientationBlocksStorageKey, JSON.stringify([...collapsedOrientationBlocks]));
    }

    function isOrientationBlockCollapsed(key) {
      return collapsedOrientationBlocks.has(key);
    }

    function toggleOrientationBlock(key) {
      if (collapsedOrientationBlocks.has(key)) {
        collapsedOrientationBlocks.delete(key);
      } else {
        collapsedOrientationBlocks.add(key);
      }
      saveCollapsedOrientationBlocks();
      render();
    }

    function appendOrientationSubsectionHeader(section, key, text) {
      const collapsed = isOrientationBlockCollapsed(key);
      const delimiter = document.createElement("div");
      delimiter.className = "orientation-delimiter";
      const label = document.createElement("span");
      label.className = "orientation-delimiter-text";
      label.textContent = text;
      const toggle = document.createElement("button");
      toggle.type = "button";
      toggle.className = "orientation-subsection-toggle";
      toggle.textContent = collapsed ? "+" : "-";
      toggle.title = collapsed ? `Expand ${text}` : `Collapse ${text}`;
      toggle.setAttribute("aria-label", toggle.title);
      toggle.setAttribute("aria-expanded", collapsed ? "false" : "true");
      toggle.addEventListener("click", () => toggleOrientationBlock(key));
      delimiter.appendChild(label);
      delimiter.appendChild(toggle);
      section.appendChild(delimiter);
      return collapsed;
    }

    function isPanelCollapsed(sectionSpec) {
      return collapsedPanels.has(panelKey(sectionSpec.title));
    }

    function togglePanelCollapsed(sectionSpec) {
      const key = panelKey(sectionSpec.title);
      if (collapsedPanels.has(key)) {
        collapsedPanels.delete(key);
      } else {
        collapsedPanels.add(key);
      }
      saveCollapsedPanels();
      render();
    }

    function createPanelTitle(sectionSpec) {
      const collapsed = isPanelCollapsed(sectionSpec);
      const title = document.createElement("h2");
      const titleText = document.createElement("span");
      titleText.className = "panel-title-text";
      titleText.textContent = sectionSpec.title;
      title.appendChild(titleText);

      const collapseButton = document.createElement("button");
      collapseButton.type = "button";
      collapseButton.className = "panel-collapse-button";
      collapseButton.textContent = collapsed ? "+" : "-";
      collapseButton.title = collapsed
        ? `Expand ${sectionSpec.title} panel`
        : `Collapse ${sectionSpec.title} panel`;
      collapseButton.setAttribute("aria-label", collapseButton.title);
      collapseButton.setAttribute("aria-expanded", collapsed ? "false" : "true");
      collapseButton.addEventListener("click", (event) => {
        event.stopPropagation();
        togglePanelCollapsed(sectionSpec);
      });
      title.appendChild(collapseButton);
      return title;
    }

    function keepTextInputTailVisible(input) {
      if (!input) {
        return;
      }
      const end = input.value.length;
      try {
        input.setSelectionRange(end, end);
      } catch (error) {
        // Some browsers can reject selection changes on transiently detached inputs.
      }
      input.scrollLeft = Number.MAX_SAFE_INTEGER;
      input.scrollLeft = input.scrollWidth;
    }

    function keepTextInputTailVisibleSoon(input) {
      requestAnimationFrame(() => keepTextInputTailVisible(input));
      setTimeout(() => keepTextInputTailVisible(input), 0);
      setTimeout(() => keepTextInputTailVisible(input), 60);
    }

    function attachTailVisiblePathBehavior(input) {
      input.classList.add("tail-visible-path");
      input.title = input.value || input.title || "Path";
      for (const eventName of ["focus", "click", "pointerup", "keyup", "input", "change", "blur"]) {
        input.addEventListener(eventName, () => {
          input.title = input.value || input.title || "Path";
          keepTextInputTailVisibleSoon(input);
        });
      }
      keepTextInputTailVisibleSoon(input);
    }

    function updateAppTitle() {
      const host = window.location.host || "local";
      const title = `Pixel To Ray Browser Config - ${host}`;
      document.title = title;
      appTitle.textContent = title;
    }

    function setCurrentProfileName(value) {
      currentProfileName = String(value || "Current").trim() || "Current";
      currentProfileNameEl.textContent = `Current: ${currentProfileName}`;
      currentProfileNameEl.title = `Current profile name: ${currentProfileName}`;
      if (!profileNameInputTouched) {
        savedProfileNameInput.value = savedProfileEditableName(suggestedSavedProfileName());
      }
    }

    function syncProfileCommentInput(force = false) {
      const comment = String(profiles.current?.[profileCommentKey] || "");
      if (force || document.activeElement !== profileCommentInput) {
        profileCommentInput.value = comment;
      }
      if (force) {
        setProfileCommentEditing(false);
      }
    }

    function setProfileCommentEditing(editing) {
      profileCommentLabel.classList.toggle("editing", editing);
    }

    function updateCurrentProfileComment(value) {
      if (!isCurrentProfileEditable()) {
        updateCurrentProfileFileUi();
        return;
      }
      if (!profiles.current) {
        profiles.current = {};
      }
      profiles.current[profileCommentKey] = String(value || "");
      dirtyProfiles.add("current");
      setStatus("Saving Current comment");
      clearTimeout(saveTimers.current);
      saveTimers.current = setTimeout(() => saveProfile("current"), 350);
    }

    function defaultSavedProfileName() {
      return `current-${new Date().toISOString().slice(0, 19).replaceAll(":", "-")}`;
    }

    function suggestedSavedProfileName() {
      return currentProfileName === "Current" ? defaultSavedProfileName() : currentProfileName;
    }

    function defaultImageSaveName() {
      return `image-${new Date().toISOString().slice(0, 19).replaceAll(":", "-")}.png`;
    }

    function activeImagePathForSave() {
      return String(profiles[activeProfile]?.fisheye_image_path || "").trim();
    }

    function suggestedImageSavePath(sourcePath = activeImagePathForSave()) {
      const normalized = normalizeHfcImageNameInput(sourcePath);
      if (normalized) {
        return normalized;
      }
      const sourceName = baseName(sourcePath);
      return sourceName || imageSavePathValue || defaultImageSaveName();
    }

    function syncImageSavePath(force = false) {
      if (force || !imageSavePathTouched) {
        imageSavePathValue = suggestedImageSavePath();
      }
    }

    function isProfileEditable(profileName) {
      return normalizeProfileName(profileName, activeProfile) === activeProfile;
    }

    function inactiveProfileTitle(profileName) {
      return `Select ${profileLabels[profileName]} before editing this column`;
    }

    function isCurrentProfileEditable() {
      return activeProfile === "current";
    }

    function currentOnlyInactiveTitle() {
      return "Switch to Current before editing Current profile files";
    }

    function updateCurrentProfileFileUi() {
      const currentEditable = isCurrentProfileEditable();
      document.querySelectorAll(".profile-current-only").forEach((element) => {
        element.classList.toggle("inactive-current", !currentEditable);
      });

      savedProfileNameInput.disabled = !currentEditable;
      profileCommentInput.disabled = !currentEditable;
      saveCurrentAsButton.disabled = !currentEditable;
      openProfileAsCurrentButton.disabled = !currentEditable || !selectedProfileFilePath;
      deleteSavedProfileButton.disabled = !currentEditable || !selectedSavedProfile();

      savedProfileNameInput.title = currentEditable ? "" : currentOnlyInactiveTitle();
      profileCommentInput.title = currentEditable ? "" : currentOnlyInactiveTitle();
      if (!currentEditable) {
        saveCurrentAsButton.title = currentOnlyInactiveTitle();
        openProfileAsCurrentButton.title = currentOnlyInactiveTitle();
        deleteSavedProfileButton.title = currentOnlyInactiveTitle();
      } else {
        if (!pendingSaveProfileName) {
          saveCurrentAsButton.title = "Save Current as assets/saved_profiles/path/name";
        }
        if (!pendingDeleteProfileName) {
          deleteSavedProfileButton.title = "Delete selected profile";
        }
        openProfileAsCurrentButton.title = selectedProfileFilePath
          ? "Open selected profile file as Current"
          : "Select a JSON profile file";
      }
    }

    async function fetchState(renderAfter = true) {
      const response = await fetch("/api/state", { cache: "no-store" });
      if (!response.ok) {
        throw new Error(`GET /api/state failed: ${response.status}`);
      }
      const payload = await response.json();
      schema = payload.schema;
      lockKeys = payload.lockKeys || {};
      profiles = payload.profiles;
      imageKeyChangesText = String(payload.imagePanelKeyChanges || imageKeyChangesText || "");
      if (!imageKeyChangesVisible || document.activeElement?.id !== "imageKeyChangesEditor") {
        imageKeyChangesDraft = imageKeyChangesText;
      }
      setCurrentProfileName(payload.currentProfileName);
      syncProfileCommentInput();
      const serverActiveProfile = normalizeProfileName(payload.activeProfile, activeProfile);
      if (!pendingActiveProfile || pendingActiveProfile === serverActiveProfile) {
        const changedActiveProfile = activeProfile !== serverActiveProfile;
        activeProfile = serverActiveProfile;
        pendingActiveProfile = "";
        if (changedActiveProfile) {
          resetImageSaveConfirmation(false);
          resetImageDeleteConfirmation(false);
          syncImageSavePath();
        }
        if (changedActiveProfile && !isCurrentProfileEditable()) {
          resetSaveConfirmation();
          resetDeleteConfirmation();
          setProfileCommentEditing(false);
        }
      }
      if (renderAfter) {
        render();
      }
      setStatus("Synced");
    }

    function render() {
      form.replaceChildren();
      syncImageSavePath();
      updateActiveProfileUi();

      for (const sectionSpec of sectionsForRender()) {
        const section = document.createElement("section");
        const panelCollapsed = isPanelCollapsed(sectionSpec);
        section.classList.toggle("collapsed", panelCollapsed);
        const title = createPanelTitle(sectionSpec);
        if (sectionSpec.title === "Image") {
          title.classList.add("panel-title-action");
          title.tabIndex = 0;
          title.title = imageKeyChangesVisible
            ? "Click to save Key Changes and close"
            : "Click to edit Image panel Key Changes";
          title.addEventListener("click", () => {
            if (panelCollapsed) {
              collapsedPanels.delete(panelKey(sectionSpec.title));
              saveCollapsedPanels();
              render();
              return;
            }
            toggleImageKeyChangesEditor();
          });
          title.addEventListener("keydown", (event) => {
            if (!["Enter", " "].includes(event.key)) {
              return;
            }
            event.preventDefault();
            if (panelCollapsed) {
              collapsedPanels.delete(panelKey(sectionSpec.title));
              saveCollapsedPanels();
              render();
              return;
            }
            toggleImageKeyChangesEditor();
          });
        }
        section.appendChild(title);
        if (panelCollapsed) {
          form.appendChild(section);
          continue;
        }
        if (sectionSpec.title === "Image" && imageKeyChangesVisible) {
          appendImageKeyChangesEditor(section);
        }
        if (sectionSpec.title !== "Image") {
          appendProfileHeaderRow(section);
        }

        for (const field of sectionSpec.fields) {
          if (sectionSpec.title !== "Image") {
            appendFieldRow(section, field);
          }
          if (sectionSpec.title === "Local Components" && field.key === "positive_y") {
            appendVaRow(section);
          }
        }
        if (sectionSpec.title === "Globe Orientation") {
          appendDerivedRotationRows(section);
        }
        if (sectionSpec.title === "Image") {
          appendImageExplorer(section);
        }
        form.appendChild(section);
      }
      updateActiveProfileUi();

      if (!imageExplorer.directory) {
        const imageDir = parentPath(profiles[activeProfile]?.fisheye_image_path || "");
        loadImageDirectory(imageDir);
      }
    }

    function sectionsForRender() {
      const imageSections = schema.filter((sectionSpec) => sectionSpec.title === "Image");
      const otherSections = schema.filter((sectionSpec) => sectionSpec.title !== "Image");
      return [...imageSections, ...otherSections];
    }

    function appendProfileHeaderRow(section) {
      const row = document.createElement("div");
      row.className = "field profile-header";
      const label = document.createElement("div");
      label.className = "profile-title";
      label.textContent = "Field";
      row.appendChild(label);
      for (const profileName of profileNames) {
        const title = document.createElement("button");
        title.type = "button";
        title.className = "profile-column-button";
        title.dataset.profile = profileName;
        title.textContent = profileLabels[profileName];
        title.addEventListener("click", () => setActiveProfile(profileName));
        row.appendChild(title);
      }
      section.appendChild(row);
    }

    function appendFieldRow(section, field) {
      const row = document.createElement("div");
      row.className = "field";
      const label = document.createElement("label");
      label.textContent = field.label;
      label.htmlFor = `current_${field.key}`;
      row.appendChild(label);

      for (const profileName of profileNames) {
        row.appendChild(createProfileFieldCell(field, profileName));
      }
      section.appendChild(row);
    }

    function createProfileFieldCell(field, profileName) {
      const state = profiles[profileName] || {};
      const editable = isProfileEditable(profileName);
      if (field.type === "boolean") {
        const cell = document.createElement("div");
        cell.className = "checkbox-cell profile-cell";
        cell.dataset.profile = profileName;
        const input = document.createElement("input");
        input.id = `${profileName}_${field.key}`;
        input.type = "checkbox";
        input.checked = Boolean(state[field.key]);
        input.disabled = !editable;
        input.title = editable ? "" : inactiveProfileTitle(profileName);
        input.addEventListener("change", () => {
          if (!isProfileEditable(profileName)) {
            input.checked = Boolean(profiles[profileName]?.[field.key]);
            return;
          }
          updateField(profileName, field.key, input.checked);
        });
        cell.appendChild(input);
        return cell;
      }

      const cell = document.createElement("div");
      cell.className = "value-cell profile-cell";
      cell.dataset.profile = profileName;
      const input = document.createElement("input");
      input.id = `${profileName}_${field.key}`;
      input.type = field.type === "number" ? "number" : "text";
      if (field.type === "number") {
        input.min = field.min;
        input.max = field.max;
        input.step = field.step;
      }
      input.value = field.type === "number" ? formatNumber(state[field.key]) : (state[field.key] ?? "");
      input.disabled = !editable;
      input.title = editable ? "" : inactiveProfileTitle(profileName);
      if (field.key === "fisheye_image_path") {
        attachTailVisiblePathBehavior(input);
      }
      input.addEventListener("input", () => {
        if (!isProfileEditable(profileName)) {
          return;
        }
        const value = field.type === "number" ? roundNumber(Number(input.value)) : input.value;
        updateField(profileName, field.key, value);
      });
      input.addEventListener("change", () => {
        if (field.type === "number") {
          input.value = formatNumber(profiles[profileName]?.[field.key]);
        }
      });
      cell.appendChild(input);

      const lockKey = lockKeys[field.key];
      if (lockKey) {
        cell.classList.add("has-lock");
        input.disabled = !editable || Boolean(state[lockKey]);
        input.title = !editable
          ? inactiveProfileTitle(profileName)
          : Boolean(state[lockKey]) ? `${field.label} is locked` : "";

        const lockButton = document.createElement("button");
        lockButton.id = `${profileName}_${lockKey}`;
        lockButton.className = "lock-toggle";
        lockButton.type = "button";
        const lockIcon = document.createElement("span");
        lockIcon.className = "lock-icon";
        lockIcon.setAttribute("aria-hidden", "true");
        lockButton.appendChild(lockIcon);
        setLockToggleState(lockButton, Boolean(state[lockKey]), field.label);
        lockButton.disabled = !editable;
        if (!editable) {
          lockButton.title = inactiveProfileTitle(profileName);
        }
        lockButton.addEventListener("click", () => {
          if (!isProfileEditable(profileName)) {
            return;
          }
          const locked = lockButton.getAttribute("aria-pressed") !== "true";
          input.disabled = locked;
          input.title = locked ? `${field.label} is locked` : "";
          setLockToggleState(lockButton, locked, field.label);
          updateField(profileName, lockKey, locked);
        });
        cell.appendChild(lockButton);
      }

      return cell;
    }

    function setLockToggleState(button, locked, label) {
      button.classList.toggle("active", locked);
      button.setAttribute("aria-pressed", locked ? "true" : "false");
      const action = locked ? "Unlock" : "Lock";
      button.title = `${action} ${label}`;
      button.setAttribute("aria-label", `${action} ${label}`);
    }

    function setActiveProfile(profileName) {
      profileName = normalizeProfileName(profileName, activeProfile);
      if (!profileNames.includes(profileName)) {
        return;
      }
      const changed = activeProfile !== profileName;
      activeProfile = profileName;
      if (changed && !isCurrentProfileEditable()) {
        resetSaveConfirmation();
        resetDeleteConfirmation();
        setProfileCommentEditing(false);
      }
      if (changed) {
        resetImageSaveConfirmation(false);
        syncImageSavePath();
      }
      updateActiveProfileUi();
      if (changed) {
        render();
      }
      saveActiveProfile(profileName);
    }

    function updateActiveProfileUi() {
      for (const profileName of profileNames) {
        const active = profileName === activeProfile;
        profileButtons[profileName]?.classList.toggle("active", active);
        profileButtons[profileName]?.setAttribute("aria-pressed", active ? "true" : "false");
      }
      document.querySelectorAll("[data-profile]").forEach((element) => {
        const active = element.dataset.profile === activeProfile;
        element.classList.toggle("active", active);
        element.classList.toggle("active-profile", active);
        element.classList.toggle("inactive-profile", !active);
      });
      updateCurrentProfileFileUi();
    }

    function normalizeProfileName(profileName, fallback = "current") {
      const normalized = String(profileName || "").toLowerCase();
      if (normalized === "default") {
        return "preset";
      }
      if (profileNames.includes(normalized)) {
        return normalized;
      }
      return profileNames.includes(fallback) ? fallback : "current";
    }

    async function saveActiveProfile(profileName) {
      pendingActiveProfile = profileName;
      try {
        const response = await fetch("/api/active-profile", {
          method: "POST",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify({ activeProfile: profileName })
        });
        if (!response.ok) {
          throw new Error(`POST active profile failed: ${response.status}`);
        }
        const payload = await response.json();
        const savedProfile = normalizeProfileName(payload.activeProfile, profileName);
        activeProfile = savedProfile;
        setCurrentProfileName(payload.currentProfileName);
        pendingActiveProfile = "";
        updateActiveProfileUi();
      } catch (error) {
        pendingActiveProfile = "";
        setStatus(error.message, true);
      }
    }

    function appendImageKeyChangesEditor(section) {
      const shell = document.createElement("div");
      shell.className = "image-key-changes-editor";

      const editor = document.createElement("textarea");
      editor.id = "imageKeyChangesEditor";
      editor.autocomplete = "off";
      editor.spellcheck = false;
      editor.value = imageKeyChangesDraft;

      const statusLine = document.createElement("div");
      statusLine.className = "vi-status-line";
      const mode = document.createElement("span");
      const hint = document.createElement("span");
      statusLine.appendChild(mode);
      statusLine.appendChild(hint);

      const setEditorMode = (nextMode, command = "") => {
        imageKeyChangesMode = nextMode;
        imageKeyChangesCommand = command;
        editor.readOnly = nextMode === "normal";
        editor.classList.toggle("normal-mode", nextMode === "normal");
        mode.textContent = nextMode === "normal" ? "-- NORMAL --" : "-- INSERT --";
        hint.textContent = command || (
          nextMode === "normal"
            ? "i insert | :wq save | :q close"
            : "Esc normal | click Image title to save"
        );
      };

      editor.addEventListener("input", () => {
        imageKeyChangesDraft = editor.value;
      });
      editor.addEventListener("keydown", (event) => {
        if (event.key === "Escape") {
          event.preventDefault();
          setEditorMode("normal");
          return;
        }
        if ((event.ctrlKey || event.metaKey) && event.key.toLowerCase() === "s") {
          event.preventDefault();
          saveImageKeyChangesAndClose();
          return;
        }
        if (imageKeyChangesMode !== "normal") {
          return;
        }

        event.preventDefault();
        if (event.key === "i") {
          setEditorMode("insert");
          return;
        }
        if (event.key === ":") {
          setEditorMode("normal", ":");
          return;
        }
        if (imageKeyChangesCommand.startsWith(":")) {
          if (event.key === "Backspace") {
            setEditorMode("normal", imageKeyChangesCommand.slice(0, -1) || "");
            return;
          }
          if (event.key === "Enter") {
            const command = imageKeyChangesCommand.trim();
            if (command === ":wq" || command === ":x") {
              saveImageKeyChangesAndClose();
            } else if (command === ":q") {
              imageKeyChangesVisible = false;
              imageKeyChangesDraft = imageKeyChangesText;
              render();
            } else {
              setEditorMode("normal", `${command} ?`);
            }
            return;
          }
          if (event.key.length === 1) {
            setEditorMode("normal", `${imageKeyChangesCommand}${event.key}`);
          }
        }
      });

      shell.appendChild(editor);
      shell.appendChild(statusLine);
      section.appendChild(shell);
      setEditorMode(imageKeyChangesMode === "normal" ? "normal" : "insert", imageKeyChangesCommand);

      requestAnimationFrame(() => {
        editor.focus({ preventScroll: true });
        if (imageKeyChangesMode !== "normal") {
          editor.setSelectionRange(editor.value.length, editor.value.length);
        }
      });
    }

    async function toggleImageKeyChangesEditor() {
      if (!imageKeyChangesVisible) {
        imageKeyChangesVisible = true;
        imageKeyChangesDraft = imageKeyChangesText;
        imageKeyChangesMode = "insert";
        imageKeyChangesCommand = "";
        render();
        return;
      }
      await saveImageKeyChangesAndClose();
    }

    async function saveImageKeyChangesAndClose() {
      imageKeyChangesText = imageKeyChangesDraft;
      try {
        setStatus("Saving Image Key Changes");
        const response = await fetch("/api/image-panel-key-changes", {
          method: "POST",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify({ text: imageKeyChangesDraft })
        });
        const payload = await response.json();
        if (!response.ok) {
          throw new Error(payload.error || `POST image key changes failed: ${response.status}`);
        }
        imageKeyChangesText = String(payload.imagePanelKeyChanges || imageKeyChangesDraft);
        imageKeyChangesDraft = imageKeyChangesText;
        imageKeyChangesVisible = false;
        imageKeyChangesMode = "insert";
        imageKeyChangesCommand = "";
        render();
        setStatus("Saved Image Key Changes");
      } catch (error) {
        setStatus(error.message, true);
      }
    }

    function appendImageExplorer(section) {
      const targetState = profiles[activeProfile] || {};
      const selectedPath = targetState.fisheye_image_path || "";
      const selectedEntry = selectedImageEntry();
      const selectedPathForAction = selectedImageEntryProfilePath(selectedEntry) || selectedImageProfilePath;
      const selectedIsHfc = selectedEntry?.kind === "hfc";
      const explorer = document.createElement("div");
      explorer.className = "image-explorer";
      explorer.appendChild(createImagePreview(selectedPath));
      explorer.appendChild(createImageSaveRow(selectedPath));

      const actions = document.createElement("div");
      actions.className = "snapshot-toolbar image-manager-actions";
      const actionLabel = document.createElement("span");
      actionLabel.className = "snapshot-toolbar-label profile-file-subtitle profile-file-browser-title";
      actionLabel.textContent = "HFC Image Browser";
      const refreshButton = document.createElement("button");
      refreshButton.type = "button";
      refreshButton.textContent = "Refresh";
      refreshButton.addEventListener("click", async () => {
        setStatus("Refreshing image files");
        await loadImageDirectory(imageExplorer.directory);
        setStatus("Image files refreshed");
      });
      const openImageButton = document.createElement("button");
      openImageButton.type = "button";
      openImageButton.textContent = "Open";
      openImageButton.disabled = !selectedPathForAction;
      openImageButton.title = selectedPathForAction
        ? `Open ${selectedPathForAction} as ${profileLabels[activeProfile]} image`
        : "Select an image file";
      openImageButton.addEventListener("click", () => openSelectedImage());
      const clearButton = document.createElement("button");
      clearButton.type = "button";
      clearButton.textContent = "Clear";
      clearButton.title = `Clear ${profileLabels[activeProfile]} image`;
      clearButton.addEventListener("click", () => selectImagePath("", activeProfile));
      const deleteImageButton = document.createElement("button");
      deleteImageButton.type = "button";
      const confirmingDeleteImage = Boolean(pendingDeleteImagePath && pendingDeleteImagePath === selectedPathForAction);
      deleteImageButton.textContent = confirmingDeleteImage
        ? "Confirm Delete"
        : "Delete";
      deleteImageButton.disabled = !selectedIsHfc;
      deleteImageButton.classList.toggle("danger", confirmingDeleteImage);
      deleteImageButton.title = selectedIsHfc
        ? `Delete ${selectedPathForAction} from assets/hfc-images`
        : "Select an HFC image file";
      deleteImageButton.addEventListener("click", () => deleteSelectedHfcImage());
      actions.appendChild(actionLabel);
      actions.appendChild(refreshButton);
      actions.appendChild(openImageButton);
      actions.appendChild(clearButton);
      actions.appendChild(deleteImageButton);
      explorer.appendChild(actions);

      const toolbar = document.createElement("div");
      toolbar.className = "image-toolbar";
      const directoryInput = document.createElement("input");
      directoryInput.id = "image_explorer_directory";
      directoryInput.type = "text";
      directoryInput.value = imageExplorer.directory || parentPath(selectedPath);
      directoryInput.placeholder = "Directory path";
      attachTailVisiblePathBehavior(directoryInput);
      directoryInput.addEventListener("keydown", (event) => {
        if (event.key === "Enter") {
          event.preventDefault();
          loadImageDirectory(directoryInput.value);
        }
      });
      const openButton = document.createElement("button");
      openButton.type = "button";
      openButton.textContent = "OpenDir";
      openButton.title = "Open the typed image directory path";
      openButton.setAttribute("aria-label", "Open image directory path");
      openButton.addEventListener("click", () => loadImageDirectory(directoryInput.value));
      const upButton = document.createElement("button");
      upButton.type = "button";
      upButton.textContent = "Up";
      upButton.disabled = !imageExplorer.parent;
      upButton.addEventListener("click", () => loadImageDirectory(imageExplorer.parent));
      toolbar.appendChild(directoryInput);
      toolbar.appendChild(openButton);
      toolbar.appendChild(upButton);
      explorer.appendChild(toolbar);

      const roots = document.createElement("div");
      roots.className = "image-root-row";
      for (const root of imageExplorer.roots || []) {
        const rootButton = document.createElement("button");
        rootButton.type = "button";
        rootButton.textContent = root.label;
        rootButton.title = root.path;
        rootButton.addEventListener("click", () => loadImageDirectory(root.path));
        roots.appendChild(rootButton);
      }
      explorer.appendChild(roots);

      const list = document.createElement("div");
      list.className = "file-list image-file-list";
      list.tabIndex = 0;
      list.setAttribute("role", "listbox");
      list.setAttribute("aria-label", "HFC image files");
      list.addEventListener("keydown", (event) => {
        if (event.target === list) {
          handleImageFileListNavigation(event);
        }
      });
      if (imageExplorer.error) {
        const message = document.createElement("div");
        message.className = "empty-list";
        message.textContent = imageExplorer.error;
        list.appendChild(message);
      } else if (!imageExplorer.entries?.length) {
        const message = document.createElement("div");
        message.className = "empty-list";
        message.textContent = "No image files or subdirectories in this directory.";
        list.appendChild(message);
      } else {
        for (const entry of imageExplorer.entries) {
          const row = document.createElement("div");
          row.className = `file-entry ${entry.type}`;
          const entryProfilePath = entry.profilePath || entry.path;
          const isSelected = isImageEntrySelected(entry, selectedPath);
          row.classList.toggle("selected", isSelected);
          row.title = entry.profilePath ? `${entry.profilePath}\n${entry.path}` : entry.path;
          row.tabIndex = 0;
          row.setAttribute("role", "option");
          row.setAttribute("aria-selected", isSelected ? "true" : "false");
          row.addEventListener("click", () => {
            if (entry.type === "directory") {
              loadImageDirectory(entry.path);
            } else {
              selectImageFileEntry(entry);
            }
          });
          row.addEventListener("dblclick", () => {
            if (entry.type === "image") {
              selectImageFileEntry(entry);
              openSelectedImage(entry);
            }
          });
          row.addEventListener("keydown", (event) => {
            if (handleImageFileListNavigation(event)) {
              return;
            }
            if (event.target !== row || !["Enter", " "].includes(event.key)) {
              return;
            }
            event.preventDefault();
            if (entry.type === "directory") {
              loadImageDirectory(entry.path);
            } else {
              selectImageFileEntry(entry);
            }
          });

          const kind = document.createElement("span");
          kind.className = "file-kind";
          kind.textContent = entry.kind || (entry.type === "directory" ? "dir" : "image");
          kind.title = imageEntryKindTitle(entry);
          const canInlineRename = entry.type === "image" && entry.kind === "hfc" && isSelected;
          const name = canInlineRename ? document.createElement("input") : document.createElement("span");
          if (canInlineRename) {
            name.type = "text";
            name.className = "file-name file-name-editor";
            name.value = entryProfilePath;
            name.title = "Edit HFC image path, press Enter to rename or Escape to cancel";
            name.addEventListener("click", (event) => event.stopPropagation());
            name.addEventListener("dblclick", (event) => event.stopPropagation());
            name.addEventListener("input", () => {
              resetImageDeleteConfirmation(false);
            });
            name.addEventListener("keydown", async (event) => {
              if (handleImageFileListNavigation(event)) {
                return;
              }
              if (event.key === "Enter") {
                event.preventDefault();
                event.stopPropagation();
                await renameSelectedHfcImage(entryProfilePath, name.value);
              } else if (event.key === "Escape") {
                event.preventDefault();
                event.stopPropagation();
                name.value = entryProfilePath;
                resetImageDeleteConfirmation();
                name.blur();
              }
            });
            name.addEventListener("blur", () => {
              name.value = entryProfilePath;
            });
          } else {
            name.className = entry.kind === "hfc" ? "file-name load-current" : "file-name";
            name.textContent = entry.name;
          }
          const size = document.createElement("span");
          size.className = "file-size";
          size.textContent = entry.type === "image" ? formatFileSize(entry.size) : "";
          row.appendChild(kind);
          row.appendChild(name);
          row.appendChild(size);
          list.appendChild(row);
        }
      }
      explorer.appendChild(list);

      const selectedNote = document.createElement("div");
      selectedNote.className = "image-note snapshot-path";
      selectedNote.textContent = selectedPathForAction
        ? `Selected image file: ${selectedPathForAction}`
        : "Selected image file: none";
      selectedNote.title = selectedEntry?.path || selectedPathForAction;
      explorer.appendChild(selectedNote);

      section.appendChild(explorer);
    }

    function createImageSaveRow(sourcePath) {
      const saveRow = document.createElement("div");
      saveRow.className = "snapshot-save-row image-save-row";

      const label = document.createElement("label");
      label.className = "profile-save-path-label";
      label.htmlFor = "hfcImageSavePathInput";
      label.textContent = "Save to assets/hfc-images/";

      const input = document.createElement("input");
      input.id = "hfcImageSavePathInput";
      input.type = "text";
      input.autocomplete = "off";
      input.placeholder = "path/image.jpg";
      input.value = imageSavePathValue || suggestedImageSavePath(sourcePath);
      input.title = "Destination below assets/hfc-images/ for saving the active image.";
      input.addEventListener("input", () => {
        imageSavePathTouched = true;
        imageSavePathValue = input.value;
        resetImageSaveConfirmation(false);
      });
      input.addEventListener("keydown", (event) => {
        if (event.key === "Enter") {
          event.preventDefault();
          saveActiveImageAsHfc(input.value);
        } else if (event.key === "Escape") {
          event.preventDefault();
          imageSavePathTouched = false;
          imageSavePathValue = suggestedImageSavePath(sourcePath);
          resetImageSaveConfirmation();
        }
      });

      const targetPath = normalizeHfcImageNameInput(input.value);
      const saveButton = document.createElement("button");
      saveButton.type = "button";
      saveButton.textContent = pendingSaveImagePath && pendingSaveImagePath === targetPath
        ? "Confirm Save"
        : "Save";
      saveButton.disabled = !sourcePath || !targetPath;
      saveButton.classList.toggle("warning", pendingSaveImagePath === targetPath);
      saveButton.title = sourcePath
        ? `Save active image as ${hfcImageAssetReference(targetPath)}`
        : "Select or open an active image before saving";
      saveButton.addEventListener("click", () => saveActiveImageAsHfc(input.value));

      saveRow.appendChild(label);
      saveRow.appendChild(input);
      saveRow.appendChild(saveButton);
      return saveRow;
    }

    function createImagePreview(selectedPath) {
      const preview = document.createElement("div");
      preview.className = "image-preview";
      if (selectedPath) {
        const img = document.createElement("img");
        img.src = `/api/image?path=${encodeURIComponent(selectedPath)}`;
        img.alt = baseName(selectedPath);
        img.addEventListener("error", () => {
          img.replaceWith(imageNote("Selected image cannot be previewed by the server."));
        });
        preview.appendChild(img);
      } else {
        preview.appendChild(imageNote("No fisheye image selected."));
      }
      return preview;
    }

    function imageNote(text) {
      const note = document.createElement("div");
      note.className = "image-note";
      note.textContent = text;
      return note;
    }

    async function loadImageDirectory(directory) {
      const requestId = ++imageExplorerRequest;
      const query = directory ? `?dir=${encodeURIComponent(directory)}` : "";
      try {
        const response = await fetch(`/api/files${query}`, { cache: "no-store" });
        const payload = await response.json();
        if (!response.ok) {
          throw new Error(payload.error || `GET /api/files failed: ${response.status}`);
        }
        if (requestId !== imageExplorerRequest) {
          return;
        }
        imageExplorer = payload;
        render();
      } catch (error) {
        if (requestId !== imageExplorerRequest) {
          return;
        }
        imageExplorer = { ...imageExplorer, error: error.message };
        render();
      }
    }

    function selectImagePath(path, profileName = activeProfile) {
      if (!isProfileEditable(profileName)) {
        setStatus(`Select ${profileLabels[profileName]} before changing its image`, true);
        render();
        return;
      }
      updateField(profileName, "fisheye_image_path", path);
      const input = document.getElementById(`${profileName}_fisheye_image_path`);
      if (input) {
        input.value = path;
      }
      resetImageSaveConfirmation(false);
      syncImageSavePath();
      render();
    }

    function selectedImageEntry() {
      const selectedFilePath = String(selectedImageFilePath || "");
      const selectedProfilePath = String(selectedImageProfilePath || "");
      if (!selectedFilePath && !selectedProfilePath) {
        return null;
      }
      return (imageExplorer.entries || []).find((entry) => {
        if (entry.type !== "image") {
          return false;
        }
        const entryProfilePath = selectedImageEntryProfilePath(entry);
        return entry.path === selectedFilePath || entryProfilePath === selectedProfilePath;
      }) || null;
    }

    function selectedImageEntryProfilePath(entry) {
      return entry ? String(entry.profilePath || entry.path || "") : "";
    }

    function isImageEntrySelected(entry, activeProfilePath = "") {
      if (entry.type !== "image") {
        return false;
      }
      const entryProfilePath = selectedImageEntryProfilePath(entry);
      return entry.path === selectedImageFilePath
        || entryProfilePath === selectedImageProfilePath
        || (!selectedImageFilePath && !selectedImageProfilePath
          && (entry.path === activeProfilePath || entryProfilePath === activeProfilePath));
    }

    function imageEntryKindTitle(entry) {
      if (entry.type === "directory") {
        return "Directory";
      }
      if (entry.kind === "hfc") {
        return "App-managed HFC image under assets/hfc-images";
      }
      return "External image file";
    }

    function selectImageFileEntry(entry) {
      if (!entry || entry.type !== "image") {
        return;
      }
      selectedImageFilePath = String(entry.path || "");
      selectedImageProfilePath = selectedImageEntryProfilePath(entry);
      resetImageDeleteConfirmation(false);
      render();
    }

    function selectableImageEntries() {
      return (imageExplorer.entries || []).filter((entry) => entry.type === "image");
    }

    function selectImageByOffset(offset) {
      const entries = selectableImageEntries();
      if (!entries.length) {
        return;
      }
      const currentIndex = entries.findIndex((entry) => {
        const entryProfilePath = selectedImageEntryProfilePath(entry);
        return entry.path === selectedImageFilePath || entryProfilePath === selectedImageProfilePath;
      });
      const fallbackIndex = offset > 0 ? -1 : entries.length;
      const baseIndex = currentIndex >= 0 ? currentIndex : fallbackIndex;
      const nextIndex = Math.max(0, Math.min(entries.length - 1, baseIndex + offset));
      const nextEntry = entries[nextIndex];
      if (!nextEntry) {
        return;
      }
      selectedImageFilePath = String(nextEntry.path || "");
      selectedImageProfilePath = selectedImageEntryProfilePath(nextEntry);
      resetImageDeleteConfirmation(false);
      requestAnimationFrame(() => {
        const selectedRow = document.querySelector(".image-file-list .file-entry.selected");
        selectedRow?.scrollIntoView({ block: "nearest" });
        selectedRow?.focus({ preventScroll: true });
      });
      render();
    }

    function handleImageFileListNavigation(event) {
      if (!["ArrowUp", "ArrowDown"].includes(event.key)) {
        return false;
      }
      event.preventDefault();
      event.stopPropagation();
      selectImageByOffset(event.key === "ArrowDown" ? 1 : -1);
      return true;
    }

    function openSelectedImage(entry = selectedImageEntry()) {
      const imagePath = selectedImageEntryProfilePath(entry) || selectedImageProfilePath;
      if (!imagePath) {
        setStatus("Select an image file before opening", true);
        render();
        return;
      }
      selectImagePath(imagePath, activeProfile);
      setStatus(`Opened ${imagePath} as ${profileLabels[activeProfile]} image`);
    }

    function resetImageSaveConfirmation(renderAfter = true) {
      pendingSaveImagePath = "";
      if (renderAfter) {
        render();
      }
    }

    function resetImageDeleteConfirmation(renderAfter = true) {
      pendingDeleteImagePath = "";
      if (renderAfter) {
        render();
      }
    }

    async function saveActiveImageAsHfc(requestedTargetPath = imageSavePathValue) {
      try {
        const sourcePath = activeImagePathForSave();
        const targetPath = normalizeHfcImageNameInput(requestedTargetPath);
        imageSavePathValue = String(requestedTargetPath || "");
        if (!sourcePath) {
          resetImageSaveConfirmation(false);
          setStatus("Open or enter an active image before saving", true);
          render();
          return;
        }
        if (!targetPath) {
          resetImageSaveConfirmation(false);
          setStatus("Enter a path below assets/hfc-images before saving", true);
          render();
          return;
        }

        if (pendingSaveImagePath !== targetPath) {
          resetImageDeleteConfirmation(false);
          pendingSaveImagePath = targetPath;
          setStatus(`Click Confirm Save to save active image as ${hfcImageAssetReference(targetPath)}`);
          render();
          return;
        }

        resetImageDeleteConfirmation(false);
        setStatus("Saving HFC image");
        const response = await fetch("/api/hfc-images/save", {
          method: "POST",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify({ sourcePath, path: targetPath, overwrite: true })
        });
        const payload = await response.json();
        if (!response.ok) {
          throw new Error(payload.error || `POST save image failed: ${response.status}`);
        }

        pendingSaveImagePath = "";
        imageSavePathTouched = false;
        imageSavePathValue = payload.profilePath || targetPath;
        selectedImageFilePath = payload.path || "";
        selectedImageProfilePath = payload.profilePath || targetPath;
        selectImagePath(selectedImageProfilePath, activeProfile);
        await loadImageDirectory(parentPath(payload.path || selectedImageFilePath) || imageExplorer.directory);
        setStatus(`Saved image as ${hfcImageAssetReference(selectedImageProfilePath)}`);
      } catch (error) {
        resetImageSaveConfirmation(false);
        setStatus(error.message, true);
        render();
      }
    }

    function imageNeighborForDelete(profilePath) {
      const entries = selectableImageEntries();
      const index = entries.findIndex((entry) => selectedImageEntryProfilePath(entry) === profilePath);
      if (index < 0) {
        return null;
      }
      return entries[index - 1] || entries[index + 1] || null;
    }

    function selectedImageMatchesProfilePath(profilePath, absolutePath = "") {
      const currentImagePath = String(profiles[activeProfile]?.fisheye_image_path || "");
      return currentImagePath === profilePath || currentImagePath === absolutePath;
    }

    async function renameSelectedHfcImage(sourcePath = selectedImageProfilePath, targetPath = selectedImageProfilePath) {
      try {
        const source = String(sourcePath || "").trim();
        const target = String(targetPath || "").trim();
        if (!source) {
          setStatus("Select an HFC image before renaming", true);
          return;
        }
        if (!target) {
          setStatus("Enter an HFC image path before renaming", true);
          return;
        }
        if (target === source) {
          setStatus("Enter a different HFC image path before renaming", true);
          return;
        }

        resetImageDeleteConfirmation(false);
        setStatus("Renaming HFC image");
        const response = await fetch("/api/hfc-images/rename", {
          method: "POST",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify({ path: source, newPath: target })
        });
        const payload = await response.json();
        if (!response.ok) {
          throw new Error(payload.error || `POST rename image failed: ${response.status}`);
        }

        selectedImageFilePath = payload.path || "";
        selectedImageProfilePath = payload.profilePath || target;
        if (selectedImageMatchesProfilePath(payload.oldProfilePath || source, payload.oldPath || "")) {
          selectImagePath(selectedImageProfilePath, activeProfile);
        }
        await loadImageDirectory(parentPath(payload.path || selectedImageFilePath) || imageExplorer.directory);
        setStatus(`Renamed ${payload.oldProfilePath || source} to ${selectedImageProfilePath}`);
      } catch (error) {
        setStatus(error.message, true);
        render();
      }
    }

    async function deleteSelectedHfcImage() {
      try {
        const entry = selectedImageEntry();
        const imagePath = selectedImageEntryProfilePath(entry) || selectedImageProfilePath;
        if (!entry || entry.kind !== "hfc" || !imagePath) {
          resetImageDeleteConfirmation(false);
          setStatus("Select an HFC image before deleting", true);
          render();
          return;
        }
        const neighborEntry = imageNeighborForDelete(imagePath);
        if (pendingDeleteImagePath !== imagePath) {
          pendingDeleteImagePath = imagePath;
          setStatus(`Click Confirm Delete to remove ${imagePath}`);
          render();
          return;
        }

        setStatus("Deleting HFC image");
        const response = await fetch("/api/hfc-images/delete", {
          method: "POST",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify({ path: imagePath })
        });
        const payload = await response.json();
        if (!response.ok) {
          throw new Error(payload.error || `POST delete image failed: ${response.status}`);
        }

        const deletedProfilePath = payload.profilePath || imagePath;
        const deletedPath = payload.path || entry.path || "";
        pendingDeleteImagePath = "";
        if (selectedImageMatchesProfilePath(deletedProfilePath, deletedPath)) {
          selectImagePath("", activeProfile);
        }
        selectedImageFilePath = neighborEntry?.path || "";
        selectedImageProfilePath = neighborEntry ? selectedImageEntryProfilePath(neighborEntry) : "";
        await loadImageDirectory(imageExplorer.directory);
        setStatus(`Deleted ${deletedProfilePath}`);
      } catch (error) {
        resetImageDeleteConfirmation(false);
        setStatus(error.message, true);
        render();
      }
    }

    function parentPath(path) {
      if (!path) {
        return "";
      }
      const normalized = String(path).replaceAll("\\", "/");
      const slashIndex = normalized.lastIndexOf("/");
      if (slashIndex <= 0) {
        return "";
      }
      return normalized.slice(0, slashIndex);
    }

    function baseName(path) {
      if (!path) {
        return "";
      }
      const normalized = String(path).replaceAll("\\", "/");
      const slashIndex = normalized.lastIndexOf("/");
      return slashIndex >= 0 ? normalized.slice(slashIndex + 1) : normalized;
    }

    function formatFileSize(size) {
      const number = Number(size);
      if (!Number.isFinite(number) || number <= 0) {
        return "";
      }
      if (number < 1024) {
        return `${number} B`;
      }
      if (number < 1024 * 1024) {
        return `${Math.round(number / 10.24) / 100} KB`;
      }
      return `${Math.round(number / 10485.76) / 100} MB`;
    }

    function profileNameFromPath(path) {
      const savedProfile = savedProfileForPath(path);
      if (savedProfile) {
        return savedProfile.name;
      }
      const normalizedPath = String(path || "").replaceAll("\\", "/");
      const assetsMarker = "/assets/";
      const assetsIndex = normalizedPath.lastIndexOf(assetsMarker);
      if (assetsIndex >= 0) {
        let relativeName = normalizedPath.slice(assetsIndex + assetsMarker.length);
        relativeName = relativeName.replace(/^saved_profiles\//i, "");
        relativeName = relativeName.replace(/\.json$/i, "");
        return relativeName;
      }
      const name = baseName(path);
      return name.toLowerCase().endsWith(".json") ? name.slice(0, -5) : name;
    }

    function normalizeProfileFileNameInput(value) {
      let name = String(value || "").trim().replaceAll("\\", "/");
      name = name.replace(/^assets\//i, "");
      name = name.replace(/^saved_profiles\//i, "");
      name = name.replace(/\/+/g, "/");
      name = name.replace(/\.json$/i, "");
      return name.replace(/^\/+|\/+$/g, "").trim();
    }

    function normalizeHfcImageNameInput(value) {
      let name = String(value || "").trim().replaceAll("\\", "/");
      const assetsHfcMarker = "/assets/hfc-images/";
      const markerIndex = name.toLowerCase().lastIndexOf(assetsHfcMarker);
      if (markerIndex >= 0) {
        name = name.slice(markerIndex + assetsHfcMarker.length);
      } else if (name.startsWith("/")) {
        name = baseName(name);
      }
      name = name.replace(/^assets\//i, "");
      name = name.replace(/^hfc-images\//i, "");
      name = name.replace(/\/+/g, "/");
      return name.replace(/^\/+|\/+$/g, "").trim();
    }

    function hfcImageAssetReference(name) {
      const normalizedName = normalizeHfcImageNameInput(name);
      return normalizedName ? `assets/hfc-images/${normalizedName}` : "assets/hfc-images/";
    }

    function savedProfileAssetReference(name) {
      const normalizedName = normalizeProfileFileNameInput(name);
      return normalizedName ? `assets/saved_profiles/${normalizedName}` : "assets/saved_profiles/";
    }

    function savedProfileEditableName(name) {
      return normalizeProfileFileNameInput(name);
    }

    function savedProfileForPath(path) {
      const normalizedPath = String(path || "").trim();
      if (!normalizedPath) {
        return null;
      }
      return savedProfiles.find((profile) => String(profile.path || "") === normalizedPath) || null;
    }

    function selectedSavedProfile() {
      return savedProfiles.find((candidate) => candidate.name === selectedSavedProfileName) || null;
    }

    function savedProfileNeighborForDelete(name) {
      const index = savedProfiles.findIndex((profile) => profile.name === name);
      if (index < 0) {
        return null;
      }
      return savedProfiles[index - 1] || savedProfiles[index + 1] || null;
    }

    function syncSavedProfileSelectionFromPath(path) {
      const profile = savedProfileForPath(path);
      const nextName = profile?.name || "";
      if (selectedSavedProfileName !== nextName) {
        resetDeleteConfirmation();
      }
      selectedSavedProfileName = nextName;
      updateSelectedSavedProfilePath();
    }

    function renderProfileFileExplorer() {
      const currentFileEditable = isCurrentProfileEditable();
      profileFileDirectoryInput.value = profileFileExplorer.directory || parentPath(selectedProfileFilePath);
      profileFileUpButton.disabled = !profileFileExplorer.parent;
      openProfileAsCurrentButton.disabled = !currentFileEditable || !selectedProfileFilePath;
      selectedProfileFilePathEl.textContent = selectedProfileFilePath
        ? `Selected browser file: ${selectedProfileFilePath}`
        : "Selected browser file: none";
      selectedProfileFilePathEl.title = selectedProfileFilePath;
      updateSelectedSavedProfilePath();

      profileFileRootRow.replaceChildren();
      for (const root of profileFileExplorer.roots || []) {
        const rootButton = document.createElement("button");
        rootButton.type = "button";
        rootButton.textContent = root.label;
        rootButton.title = root.path;
        rootButton.addEventListener("click", () => loadProfileFileDirectory(root.path));
        profileFileRootRow.appendChild(rootButton);
      }

      profileFileList.replaceChildren();
      if (profileFileExplorer.error) {
        const message = document.createElement("div");
        message.className = "empty-list";
        message.textContent = profileFileExplorer.error;
        profileFileList.appendChild(message);
        updateCurrentProfileFileUi();
        return;
      }
      if (!profileFileExplorer.entries?.length) {
        const message = document.createElement("div");
        message.className = "empty-list";
        message.textContent = "No JSON profile files or subdirectories in this directory.";
        profileFileList.appendChild(message);
        updateCurrentProfileFileUi();
        return;
      }

      for (const entry of profileFileExplorer.entries) {
        const savedProfile = entry.type === "profile" ? savedProfileForPath(entry.path) : null;
        const isSelected = entry.path === selectedProfileFilePath;
        const row = document.createElement("div");
        row.className = `file-entry ${entry.type}`;
        row.classList.toggle("selected", isSelected);
        row.title = entry.path;
        row.tabIndex = 0;
        row.setAttribute("role", "option");
        row.setAttribute("aria-selected", isSelected ? "true" : "false");
        row.addEventListener("click", () => {
          if (entry.type === "directory") {
            loadProfileFileDirectory(entry.path);
          } else {
            selectProfileFilePath(entry.path);
          }
        });
        row.addEventListener("dblclick", () => {
          if (entry.type === "profile" && currentFileEditable) {
            openProfileFileAsCurrent(entry.path);
          }
        });
        row.addEventListener("keydown", (event) => {
          if (handleProfileFileListNavigation(event)) {
            return;
          }
          if (event.target !== row || !["Enter", " "].includes(event.key)) {
            return;
          }
          event.preventDefault();
          if (entry.type === "directory") {
            loadProfileFileDirectory(entry.path);
          } else {
            selectProfileFilePath(entry.path);
          }
        });

        const kind = document.createElement("span");
        kind.className = "file-kind";
        kind.textContent = entry.type === "directory" ? "dir" : savedProfile ? "profile" : "json";
        kind.title = entry.type === "directory"
          ? "Directory"
          : savedProfile
            ? "App-managed profile under assets/saved_profiles"
            : "Regular JSON file";
        const name = savedProfile && isSelected && currentFileEditable
          ? document.createElement("input")
          : document.createElement("span");
        if (savedProfile && isSelected && currentFileEditable) {
          name.type = "text";
          name.className = "file-name file-name-editor";
          name.value = savedProfileEditableName(savedProfile.name);
          name.title = "Edit name, press Enter to rename or Escape to cancel";
          name.addEventListener("click", (event) => event.stopPropagation());
          name.addEventListener("dblclick", (event) => event.stopPropagation());
          name.addEventListener("input", () => {
            resetDeleteConfirmation();
          });
          name.addEventListener("keydown", async (event) => {
            if (handleProfileFileListNavigation(event)) {
              return;
            }
            if (event.key === "Enter") {
              event.preventDefault();
              event.stopPropagation();
              const targetName = normalizeProfileFileNameInput(name.value);
              await renameSavedProfile(savedProfile.name, targetName);
            } else if (event.key === "Escape") {
              event.preventDefault();
              event.stopPropagation();
              name.value = savedProfileEditableName(savedProfile.name);
              resetDeleteConfirmation();
              name.blur();
            }
          });
          name.addEventListener("blur", () => {
            name.value = savedProfileEditableName(savedProfile.name);
          });
        } else {
          name.className = savedProfile ? "file-name load-current" : "file-name";
          name.textContent = entry.name;
        }
        const size = document.createElement("span");
        size.className = "file-size";
        size.textContent = entry.type === "profile" ? formatFileSize(entry.size) : "";
        row.appendChild(kind);
        row.appendChild(name);
        row.appendChild(size);
        profileFileList.appendChild(row);
      }
      updateCurrentProfileFileUi();
    }

    function selectableProfileFileEntries() {
      return (profileFileExplorer.entries || []).filter((entry) => entry.type === "profile");
    }

    function selectProfileFileByOffset(offset) {
      const entries = selectableProfileFileEntries();
      if (!entries.length) {
        return;
      }
      const currentIndex = entries.findIndex((entry) => entry.path === selectedProfileFilePath);
      const fallbackIndex = offset > 0 ? -1 : entries.length;
      const baseIndex = currentIndex >= 0 ? currentIndex : fallbackIndex;
      const nextIndex = Math.max(0, Math.min(entries.length - 1, baseIndex + offset));
      const nextEntry = entries[nextIndex];
      if (!nextEntry) {
        return;
      }
      selectProfileFilePath(nextEntry.path);
      requestAnimationFrame(() => {
        const selectedRow = profileFileList.querySelector(".file-entry.selected");
        selectedRow?.scrollIntoView({ block: "nearest" });
        if (document.activeElement !== savedProfileNameInput) {
          selectedRow?.focus({ preventScroll: true });
        }
      });
    }

    function handleProfileFileListNavigation(event) {
      if (!["ArrowUp", "ArrowDown"].includes(event.key)) {
        return false;
      }
      event.preventDefault();
      event.stopPropagation();
      selectProfileFileByOffset(event.key === "ArrowDown" ? 1 : -1);
      return true;
    }

    async function loadProfileFileDirectory(directory) {
      const requestId = ++profileFileExplorerRequest;
      const query = directory ? `?dir=${encodeURIComponent(directory)}` : "";
      try {
        const response = await fetch(`/api/profile-files${query}`, { cache: "no-store" });
        const payload = await response.json();
        if (!response.ok) {
          throw new Error(payload.error || `GET /api/profile-files failed: ${response.status}`);
        }
        if (requestId !== profileFileExplorerRequest) {
          return;
        }
        profileFileExplorer = payload;
        renderProfileFileExplorer();
      } catch (error) {
        if (requestId !== profileFileExplorerRequest) {
          return;
        }
        profileFileExplorer = { ...profileFileExplorer, error: error.message };
        renderProfileFileExplorer();
      }
    }

    async function refreshProfileFileExplorerForPath(path) {
      const normalizedPath = String(path || "").trim();
      if (normalizedPath) {
        selectedProfileFilePath = normalizedPath;
        syncSavedProfileSelectionFromPath(normalizedPath);
        await loadProfileFileDirectory(parentPath(normalizedPath));
        return;
      }
      await loadProfileFileDirectory(profileFileExplorer.directory);
    }

    function selectProfileFilePath(path) {
      selectedProfileFilePath = path;
      syncSavedProfileSelectionFromPath(path);
      renderProfileFileExplorer();
    }

    async function openProfileFileAsCurrent(requestedPath = selectedProfileFilePath) {
      try {
        if (!isCurrentProfileEditable()) {
          setStatus("Switch to Current before opening a profile file as Current", true);
          updateCurrentProfileFileUi();
          return;
        }
        const path = String(requestedPath || "").trim();
        if (!path) {
          setStatus("Select a JSON profile file before opening", true);
          return;
        }
        if (dirtyProfiles.has("current")) {
          setStatus("Current has pending edits; wait for save before opening a profile file", true);
          return;
        }

        resetSaveConfirmation();
        clearTimeout(saveTimers.current);
        delete saveTimers.current;
        dirtyProfiles.delete("current");
        setStatus("Opening profile file as Current");
        const response = await fetch("/api/profile-files/current/open", {
          method: "POST",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify({ path })
        });
        const payload = await response.json();
        if (!response.ok) {
          throw new Error(payload.error || `POST open-profile-file failed: ${response.status}`);
        }

        selectedProfileFilePath = payload.path || path;
        profiles.current = payload.profile;
        activeProfile = normalizeProfileName(payload.activeProfile, "current");
        setCurrentProfileName(payload.currentProfileName || payload.name);
        syncProfileCommentInput(true);
        savedProfileNameInput.value = savedProfileEditableName(payload.name || profileNameFromPath(path));
        profileNameInputTouched = false;
        await refreshSavedProfiles(payload.name);
        syncSavedProfileSelectionFromPath(selectedProfileFilePath);
        pendingActiveProfile = "";
        renderProfileFileExplorer();
        render();
        setStatus(`Opened ${payload.name || profileNameFromPath(path)} as Current`);
      } catch (error) {
        setStatus(error.message, true);
      }
    }

    function appendDerivedRotationRows(section) {
      const eulerCollapsed = appendOrientationSubsectionHeader(
        section,
        "euler-axis-angle",
        "Euler axis-angle equivalent | read-only"
      );
      if (!eulerCollapsed) {
        appendDerivedRow(section, "Euler axis_x", "euler_axis_x", "axisX");
        appendDerivedRow(section, "Euler axis_y", "euler_axis_y", "axisY");
        appendDerivedRow(section, "Euler axis_z", "euler_axis_z", "axisZ");
        appendDerivedRow(section, "Euler angle deg", "euler_angle_deg", "angleDegrees");
      }

      const localAxisCollapsed = appendOrientationSubsectionHeader(
        section,
        "local-axis-rotations",
        "Local-axis rotations | type degrees and press Enter | Enter applies once"
      );
      if (!localAxisCollapsed) {
        appendOrientationCommandRow(section, "Roll (Z) deg", "globe_roll_z_delta", "z", "Roll");
        appendOrientationCommandRow(section, "Yaw (Y) deg", "globe_yaw_y_delta", "y", "Yaw");
        appendOrientationCommandRow(section, "Pitch (X) deg", "globe_pitch_x_delta", "x", "Pitch");
      }
    }

    function appendDerivedRow(section, labelText, key, valueKey) {
      const row = document.createElement("div");
      row.className = "field derived";
      const label = document.createElement("label");
      label.textContent = labelText;
      label.htmlFor = `current_${key}`;
      row.appendChild(label);

      for (const profileName of profileNames) {
        const values = axisAngleFromState(profiles[profileName] || {});
        const input = document.createElement("input");
        input.id = `${profileName}_${key}`;
        input.type = "number";
        input.className = "profile-derived-cell";
        input.dataset.profile = profileName;
        input.value = formatNumber(values[valueKey]);
        input.disabled = true;
        row.appendChild(input);
      }
      section.appendChild(row);
    }

    function appendOrientationCommandRow(section, labelText, key, axis, commandName) {
      const row = document.createElement("div");
      row.className = "field orientation-command";
      const label = document.createElement("label");
      label.textContent = labelText;
      label.htmlFor = `current_${key}`;
      row.appendChild(label);

      for (const profileName of profileNames) {
        const state = profiles[profileName] || {};
        const editable = isProfileEditable(profileName);
        const locked = isOrientationLocked(state);
        const input = document.createElement("input");
        input.id = `${profileName}_${key}`;
        input.type = "text";
        input.dataset.profile = profileName;
        input.inputMode = "decimal";
        input.autocomplete = "off";
        input.value = "0";
        input.disabled = !editable || locked;
        input.title = !editable
          ? inactiveProfileTitle(profileName)
          : locked
            ? "Unlock Globe q_w/q_x/q_y/q_z before applying incremental rotations"
            : `${commandName}: rotate around local ${axis.toUpperCase()} once on Enter`;
        input.addEventListener("keydown", (event) => {
          if (event.key === "Enter") {
            event.preventDefault();
            applyOrientationCommand(profileName, axis, commandName, input);
          } else if (event.key === "Escape") {
            input.value = "0";
          }
        });
        row.appendChild(input);
      }
      section.appendChild(row);
    }

    function axisAngleFromState(state) {
      const quaternion = normalizedQuaternionFromState(state);
      if (!quaternion) {
        return { axisX: 0, axisY: 0, axisZ: 1, angleDegrees: 0 };
      }
      let { w, x, y, z } = quaternion;

      if (w < 0) {
        w = -w;
        x = -x;
        y = -y;
        z = -z;
      }

      const vectorLength = Math.hypot(x, y, z);
      if (vectorLength <= 1e-8) {
        return { axisX: 0, axisY: 0, axisZ: 1, angleDegrees: 0 };
      }

      return {
        axisX: roundNumber(x / vectorLength),
        axisY: roundNumber(y / vectorLength),
        axisZ: roundNumber(z / vectorLength),
        angleDegrees: roundNumber(2 * Math.atan2(vectorLength, w) * 180 / Math.PI)
      };
    }

    function normalizedQuaternionFromState(state) {
      let w = Number(state.globe_orientation_scalar ?? 1);
      let x = Number(state.globe_orientation_x ?? 0);
      let y = Number(state.globe_orientation_y ?? 0);
      let z = Number(state.globe_orientation_z ?? 0);
      if (![w, x, y, z].every(Number.isFinite)) {
        return null;
      }

      const length = Math.hypot(w, x, y, z);
      if (length <= 1e-8) {
        return null;
      }
      return { w: w / length, x: x / length, y: y / length, z: z / length };
    }

    function normalizeQuaternion(quaternion) {
      const length = Math.hypot(quaternion.w, quaternion.x, quaternion.y, quaternion.z);
      if (!Number.isFinite(length) || length <= 1e-8) {
        return { w: 1, x: 0, y: 0, z: 0 };
      }
      return {
        w: quaternion.w / length,
        x: quaternion.x / length,
        y: quaternion.y / length,
        z: quaternion.z / length
      };
    }

    function degreesToRadians(value) {
      return value * Math.PI / 180;
    }

    function isOrientationLocked(state) {
      return orientationLockKeys.some((key) => Boolean(state[key]));
    }

    function updateDerivedRotationValues(profileName) {
      const values = axisAngleFromState(profiles[profileName] || {});
      const pairs = [
        ["euler_axis_x", values.axisX],
        ["euler_axis_y", values.axisY],
        ["euler_axis_z", values.axisZ],
        ["euler_angle_deg", values.angleDegrees]
      ];
      for (const [id, value] of pairs) {
        const input = document.getElementById(`${profileName}_${id}`);
        if (input) {
          input.value = formatNumber(value);
        }
      }
    }

    function updateOrientationCommandValues(profileName) {
      const state = profiles[profileName] || {};
      const locked = isOrientationLocked(state);
      const editable = isProfileEditable(profileName);
      const ids = ["globe_roll_z_delta", "globe_yaw_y_delta", "globe_pitch_x_delta"];
      for (const id of ids) {
        const input = document.getElementById(`${profileName}_${id}`);
        if (!input) {
          continue;
        }
        if (document.activeElement !== input) {
          input.value = "0";
        }
        input.disabled = !editable || locked;
        input.title = !editable
          ? inactiveProfileTitle(profileName)
          : locked
            ? "Unlock Globe q_w/q_x/q_y/q_z before applying incremental rotations"
            : "Type degrees and press Enter to apply one local-axis rotation";
      }
    }

    function updateOrientationQuaternionInputs(profileName) {
      const state = profiles[profileName] || {};
      for (const key of orientationKeys) {
        const input = document.getElementById(`${profileName}_${key}`);
        if (input && document.activeElement !== input) {
          input.value = formatNumber(state[key]);
        }
      }
    }

    function applyOrientationCommand(profileName, axis, commandName, input) {
      if (!isProfileEditable(profileName)) {
        updateOrientationCommandValues(profileName);
        setStatus(`Select ${profileLabels[profileName]} before applying ${commandName}`, true);
        return;
      }
      if (!profiles[profileName]) {
        profiles[profileName] = {};
      }
      if (isOrientationLocked(profiles[profileName])) {
        updateOrientationCommandValues(profileName);
        return;
      }

      const degrees = Number(input.value);
      if (!Number.isFinite(degrees)) {
        setStatus(`${commandName} needs a numeric degree value`, true);
        input.select();
        return;
      }
      if (Math.abs(degrees) <= 1e-8) {
        setStatus(`${commandName} ignored: 0 degrees`);
        return;
      }

      const current = normalizedQuaternionFromState(profiles[profileName]) || { w: 1, x: 0, y: 0, z: 0 };
      const delta = axisQuaternion(axis, degrees);
      const quaternion = multiplyQuaternions(current, delta);
      setProfileQuaternion(profileName, quaternion);
      updateOrientationQuaternionInputs(profileName);
      updateDerivedRotationValues(profileName);
      updateOrientationCommandValues(profileName);
      setStatus(`${commandName} ${roundNumber(degrees)} deg applied to ${profileLabels[profileName]}`);
      queueProfileSave(profileName);
    }

    function axisQuaternion(axis, degrees) {
      const halfRadians = degreesToRadians(degrees) * 0.5;
      const scalar = Math.cos(halfRadians);
      const vector = Math.sin(halfRadians);
      return normalizeQuaternion({
        w: scalar,
        x: axis === "x" ? vector : 0,
        y: axis === "y" ? vector : 0,
        z: axis === "z" ? vector : 0
      });
    }

    function multiplyQuaternions(a, b) {
      return normalizeQuaternion({
        w: a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z,
        x: a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
        y: a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
        z: a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w
      });
    }

    function setProfileQuaternion(profileName, quaternion) {
      const normalized = normalizeQuaternion(quaternion);
      profiles[profileName].globe_orientation_scalar = roundNumber(normalized.w);
      profiles[profileName].globe_orientation_x = roundNumber(normalized.x);
      profiles[profileName].globe_orientation_y = roundNumber(normalized.y);
      profiles[profileName].globe_orientation_z = roundNumber(normalized.z);
    }

    function appendVaRow(section) {
      const row = document.createElement("div");
      row.className = "field derived";
      const label = document.createElement("label");
      label.textContent = "va";
      label.htmlFor = "current_va";
      row.appendChild(label);

      for (const profileName of profileNames) {
        const input = document.createElement("input");
        input.id = `${profileName}_va`;
        input.type = "number";
        input.className = "profile-derived-cell";
        input.dataset.profile = profileName;
        input.value = formatNumber(vaAngleFromState(profiles[profileName] || {}));
        input.disabled = true;
        row.appendChild(input);
      }
      section.appendChild(row);
    }

    function vaAngleFromState(state) {
      const positiveY = Number(state.positive_y ?? 0);
      const globeRadius = Number(state.globe_radius ?? 1);
      if (!Number.isFinite(positiveY) || !Number.isFinite(globeRadius) || globeRadius <= 0) {
        return 0;
      }
      return roundNumber(Math.atan2(Math.max(0, positiveY), globeRadius) * 180 / Math.PI);
    }

    function updateVaValue(profileName) {
      const input = document.getElementById(`${profileName}_va`);
      if (input) {
        input.value = formatNumber(vaAngleFromState(profiles[profileName] || {}));
      }
    }

    function roundNumber(value) {
      return Number.isFinite(value) ? Math.round(value * 100) / 100 : value;
    }

    function formatNumber(value) {
      if (value === undefined || value === null || value === "") {
        return "";
      }
      const number = Number(value);
      return Number.isFinite(number) ? String(roundNumber(number)) : "";
    }

    function updateField(profileName, key, value) {
      if (!isProfileEditable(profileName)) {
        setStatus(`Select ${profileLabels[profileName]} before editing this column`, true);
        render();
        return;
      }
      if (!profiles[profileName]) {
        profiles[profileName] = {};
      }
      profiles[profileName][key] = value;
      if (orientationKeys.has(key)) {
        updateDerivedRotationValues(profileName);
        updateOrientationCommandValues(profileName);
      }
      if (orientationLockKeys.includes(key)) {
        updateOrientationCommandValues(profileName);
      }
      if (vaKeys.has(key)) {
        updateVaValue(profileName);
      }
      queueProfileSave(profileName);
    }

    function queueProfileSave(profileName) {
      dirtyProfiles.add(profileName);
      setStatus(`Saving ${profileLabels[profileName]}`);
      clearTimeout(saveTimers[profileName]);
      saveTimers[profileName] = setTimeout(() => saveProfile(profileName), 250);
    }

    async function saveProfile(profileName) {
      try {
        const response = await fetch(`/api/state/${profileName}`, {
          method: "POST",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify(profiles[profileName] || {})
        });
        if (!response.ok) {
          throw new Error(`POST failed: ${response.status}`);
        }
        const payload = await response.json();
        profiles[profileName] = payload.profile;
        updateDerivedRotationValues(profileName);
        updateOrientationCommandValues(profileName);
        updateVaValue(profileName);
        if (profileName === "current") {
          syncProfileCommentInput();
        }
        dirtyProfiles.delete(profileName);
        setStatus(dirtyProfiles.size ? "Saving" : `Saved ${profileLabels[profileName]}`);
        return true;
      } catch (error) {
        setStatus(error.message, true);
        return false;
      }
    }

    async function saveCurrentAs() {
      if (!isCurrentProfileEditable()) {
        resetSaveConfirmation();
        setStatus("Switch to Current before saving Current profile files", true);
        updateCurrentProfileFileUi();
        return;
      }
      const name = normalizeProfileFileNameInput(savedProfileNameInput.value);
      if (!name) {
        savedProfileNameInput.focus();
        resetSaveConfirmation();
        setStatus("Enter a path/name below assets/saved_profiles before saving", true);
        return;
      }
      const displayName = savedProfileAssetReference(name);
      if (pendingSaveProfileName !== name) {
        resetDeleteConfirmation();
        pendingSaveProfileName = name;
        saveCurrentAsButton.textContent = "Confirm Save";
        saveCurrentAsButton.classList.add("warning");
        saveCurrentAsButton.title = `Save Current as ${displayName}`;
        setStatus(`Click Confirm Save to save Current as ${displayName}`);
        return;
      }

      clearTimeout(saveTimers.current);
      if (dirtyProfiles.has("current")) {
        const saved = await saveProfile("current");
        if (!saved) {
          return;
        }
      }

      try {
        setStatus("Saving Current snapshot");
        const response = await fetch("/api/saved-profiles/current", {
          method: "POST",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify({ name })
        });
        const payload = await response.json();
        if (!response.ok) {
          throw new Error(payload.error || `POST save-as failed: ${response.status}`);
        }
        setCurrentProfileName(payload.currentProfileName || payload.name);
        savedProfileNameInput.value = savedProfileEditableName(payload.name);
        profileNameInputTouched = false;
        resetSaveConfirmation();
        await refreshSavedProfiles(payload.name, true);
        await refreshProfileFileExplorerForPath(payload.path);
        setStatus(`Saved Current as ${savedProfileAssetReference(payload.name)}`);
      } catch (error) {
        resetSaveConfirmation();
        setStatus(error.message, true);
      }
    }

    async function fetchSavedProfiles() {
      const response = await fetch("/api/saved-profiles", { cache: "no-store" });
      const payload = await response.json();
      if (!response.ok) {
        throw new Error(payload.error || `GET profile list failed: ${response.status}`);
      }
      return payload;
    }

    async function refreshSavedProfiles(
      preferredName = selectedSavedProfileName,
      selectPreferredFile = false,
      allowDefaultFallback = true
    ) {
      const requestId = ++savedProfilesRequest;
      const payload = await fetchSavedProfiles();
      if (requestId !== savedProfilesRequest) {
        return;
      }
      resetSaveConfirmation();
      resetDeleteConfirmation();
      savedProfiles = Array.isArray(payload.profiles) ? payload.profiles : [];
      const preferred = String(preferredName || "").trim();
      const previous = selectedSavedProfileName;
      const preferredProfile = savedProfiles.find((profile) => profile.name === preferred) || null;
      const selectedPathProfile = savedProfileForPath(selectedProfileFilePath);
      let nextProfile = null;
      if (selectPreferredFile && preferredProfile) {
        nextProfile = preferredProfile;
        selectedProfileFilePath = preferredProfile.path || selectedProfileFilePath;
      } else if (selectedPathProfile) {
        nextProfile = selectedPathProfile;
      } else if (!selectedProfileFilePath && allowDefaultFallback) {
        nextProfile = preferredProfile
          || savedProfiles.find((profile) => profile.name === previous)
          || savedProfiles.find((profile) => profile.name === currentProfileName)
          || savedProfiles[0]
          || null;
        if (nextProfile?.path) {
          selectedProfileFilePath = nextProfile.path;
        }
      }
      selectedSavedProfileName = nextProfile?.name || "";
      updateSelectedSavedProfilePath();
      renderProfileFileExplorer();
    }

    function updateSelectedSavedProfilePath() {
      const profile = selectedSavedProfile();
      deleteSavedProfileButton.disabled = !isCurrentProfileEditable() || !profile;
      if (!profile) {
        resetDeleteConfirmation();
      }
      updateCurrentProfileFileUi();
    }

    function resetSaveConfirmation() {
      pendingSaveProfileName = "";
      saveCurrentAsButton.textContent = "Save";
      saveCurrentAsButton.classList.remove("warning");
      saveCurrentAsButton.title = "Save Current as assets/saved_profiles/path/name";
      updateCurrentProfileFileUi();
    }

    function resetDeleteConfirmation() {
      pendingDeleteProfileName = "";
      deleteSavedProfileButton.textContent = "Delete";
      deleteSavedProfileButton.classList.remove("danger");
      deleteSavedProfileButton.title = "Delete selected profile";
      updateCurrentProfileFileUi();
    }

    async function renameSavedProfile(
      requestedSourceName = selectedSavedProfileName,
      requestedTargetName = selectedSavedProfileName
    ) {
      try {
        if (!isCurrentProfileEditable()) {
          setStatus("Switch to Current before renaming Current profile files", true);
          updateCurrentProfileFileUi();
          return;
        }
        const sourceName = String(requestedSourceName || "").trim();
        if (!sourceName) {
          setStatus("Select a profile before renaming", true);
          return;
        }

        const targetName = normalizeProfileFileNameInput(requestedTargetName);
        if (!targetName) {
          setStatus("Enter a profile filename in the selected row before renaming", true);
          return;
        }
        if (targetName === sourceName) {
          setStatus("Enter a different profile filename in the selected row before renaming", true);
          return;
        }

        resetSaveConfirmation();
        resetDeleteConfirmation();
        setStatus("Renaming profile");
        const response = await fetch("/api/saved-profiles/current/rename", {
          method: "POST",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify({ name: sourceName, newName: targetName })
        });
        const payload = await response.json();
        if (!response.ok) {
          throw new Error(payload.error || `POST rename profile failed: ${response.status}`);
        }
        const renamedName = payload.name || payload.newName || targetName;
        setCurrentProfileName(payload.currentProfileName);
        selectedSavedProfileName = renamedName;
        resetSaveConfirmation();
        await refreshSavedProfiles(renamedName, true);
        await refreshProfileFileExplorerForPath(payload.path);
        setStatus(`Renamed ${savedProfileAssetReference(payload.oldName || sourceName)} to ${savedProfileAssetReference(renamedName)}`);
      } catch (error) {
        resetSaveConfirmation();
        setStatus(error.message, true);
      }
    }

    async function deleteSavedProfile() {
      try {
        if (!isCurrentProfileEditable()) {
          resetDeleteConfirmation();
          setStatus("Switch to Current before deleting Current profile files", true);
          updateCurrentProfileFileUi();
          return;
        }
        const name = selectedSavedProfileName;
        if (!name) {
          setStatus("Select a profile before deleting", true);
          return;
        }
        const profile = selectedSavedProfile();
        const deletedPath = profile?.path || "";
        const neighborProfile = savedProfileNeighborForDelete(name);
        const neighborName = neighborProfile?.name || "";
        const neighborPath = neighborProfile?.path || "";
        if (pendingDeleteProfileName !== name) {
          resetSaveConfirmation();
          pendingDeleteProfileName = name;
          deleteSavedProfileButton.textContent = "Confirm Delete";
          deleteSavedProfileButton.classList.add("danger");
          deleteSavedProfileButton.title = `Delete ${name}`;
          setStatus(`Click Confirm Delete to remove ${name}`);
          return;
        }

        setStatus("Deleting profile");
        const response = await fetch("/api/saved-profiles/current/delete", {
          method: "POST",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify({ name })
        });
        const payload = await response.json();
        if (!response.ok) {
          throw new Error(payload.error || `POST delete profile failed: ${response.status}`);
        }
        const deletedName = payload.name || name;
        resetDeleteConfirmation();
        selectedSavedProfileName = selectedSavedProfileName === deletedName ? neighborName : selectedSavedProfileName;
        if (selectedProfileFilePath === deletedPath) {
          selectedProfileFilePath = neighborPath;
        }
        await refreshSavedProfiles(neighborName, Boolean(neighborName), Boolean(neighborName));
        await loadProfileFileDirectory(profileFileExplorer.directory);
        setStatus(`Deleted ${deletedName}`);
      } catch (error) {
        resetDeleteConfirmation();
        setStatus(error.message, true);
      }
    }

    async function reloadNow() {
      try {
        dirtyProfiles = new Set();
        for (const profileName of profileNames) {
          clearTimeout(saveTimers[profileName]);
        }
        saveTimers = {};
        await fetchState(true);
        await refreshSavedProfiles();
        await loadProfileFileDirectory(profileFileExplorer.directory);
        await loadImageDirectory(imageExplorer.directory);
      } catch (error) {
        setStatus(error.message, true);
      }
    }

    savedProfileNameInput.value = savedProfileEditableName(suggestedSavedProfileName());
    savedProfileNameInput.addEventListener("input", () => {
      profileNameInputTouched = true;
      resetSaveConfirmation();
      resetDeleteConfirmation();
    });
    profileCommentInput.addEventListener("focus", () => setProfileCommentEditing(true));
    profileCommentInput.addEventListener("input", () => {
      setProfileCommentEditing(true);
      updateCurrentProfileComment(profileCommentInput.value);
    });
    profileCommentInput.addEventListener("blur", () => setProfileCommentEditing(false));
    saveCurrentAsButton.addEventListener("click", saveCurrentAs);
    profileFileDirectoryInput.addEventListener("keydown", (event) => {
      if (event.key === "Enter") {
        event.preventDefault();
        loadProfileFileDirectory(profileFileDirectoryInput.value);
      }
    });
    openProfileDirectoryButton.addEventListener("click", () => loadProfileFileDirectory(profileFileDirectoryInput.value));
    profileFileUpButton.addEventListener("click", () => loadProfileFileDirectory(profileFileExplorer.parent));
    openProfileAsCurrentButton.addEventListener("click", () => openProfileFileAsCurrent());
    profileFileList.addEventListener("keydown", (event) => {
      if (event.target === profileFileList) {
        handleProfileFileListNavigation(event);
      }
    });
    deleteSavedProfileButton.addEventListener("click", deleteSavedProfile);
    refreshSavedProfilesButton.addEventListener("click", async () => {
      try {
        setStatus("Refreshing profile files");
        await refreshSavedProfiles();
        await loadProfileFileDirectory(profileFileExplorer.directory);
        setStatus("Profile files refreshed");
      } catch (error) {
        setStatus(error.message, true);
      }
    });
    reloadButton.addEventListener("click", reloadNow);
    profileFileCollapseButton.addEventListener("click", toggleProfileFilePanelCollapsed);
    for (const profileName of profileNames) {
      profileButtons[profileName]?.addEventListener("click", () => setActiveProfile(profileName));
    }

    setInterval(async () => {
      if (dirtyProfiles.size || ["INPUT", "TEXTAREA"].includes(document.activeElement?.tagName || "")) {
        return;
      }
      try {
        await fetchState(true);
      } catch (error) {
        setStatus(error.message, true);
      }
    }, 1500);

    updateAppTitle();
    updateProfileFilePanelCollapseUi();
    renderProfileFileExplorer();
    reloadNow();
  </script>
</body>
</html>
"""


def default_profile(assets_dir: Path) -> dict[str, Any]:
    hfc_circular_path = assets_dir / HFC_IMAGES_DIR / "cj-circular.png"
    testing_circular_path = assets_dir / "testing-images" / "cj-circular.png"
    fisheye_path = "cj-circular.png" if hfc_circular_path.exists() else (
        str(testing_circular_path.resolve()) if testing_circular_path.exists() else ""
    )
    return {
        "latitude": 20.0,
        "longitude": 35.0,
        "roll": 0.0,
        "zoom": 1.0,
        "cap_polar_angle": 180.0,
        "globe_radius": 1.0,
        "globe_orientation_scalar": 1.0,
        "globe_orientation_x": 0.0,
        "globe_orientation_y": 0.0,
        "globe_orientation_z": 0.0,
        "fisheye_image_path": fisheye_path,
        "perspective_fov_y": 35.0,
        "camera_eye_offset_x": 4.0,
        "camera_eye_offset_y": 4.0,
        "camera_eye_offset_z": 4.0,
        "camera_up_x": 0.0,
        "camera_up_y": 0.0,
        "camera_up_z": 1.0,
        "rear_hemisphere_wireframe_visible": True,
        "world_axes_visible": True,
        "local_coordinate_frame_visible": True,
        "global_center_fixed": True,
        "panorama_band_visible": True,
        "x_tangent_cylinder_visible": True,
        "surface_depth_hiding_enabled": True,
        "cap_closed": True,
        "perspective_projection": False,
        "positive_x": 0.5,
        "negtive_x": 0.3,
        "positive_y": 0.5,
        "negtive_y": 0.3,
        "positive_z": 0.5,
        "latitude_locked": False,
        "longitude_locked": False,
        "roll_locked": False,
        "zoom_locked": False,
        "cap_polar_angle_locked": False,
        "globe_radius_locked": True,
        "globe_orientation_scalar_locked": False,
        "globe_orientation_x_locked": False,
        "globe_orientation_y_locked": False,
        "globe_orientation_z_locked": False,
        "camera_eye_offset_x_locked": False,
        "camera_eye_offset_y_locked": False,
        "camera_eye_offset_z_locked": False,
        "camera_up_x_locked": False,
        "camera_up_y_locked": False,
        "camera_up_z_locked": False,
        "positive_x_locked": False,
        "negtive_x_locked": False,
        "positive_y_locked": False,
        "negtive_y_locked": False,
        "positive_z_locked": False,
    }


def read_json_file(path: Path) -> dict[str, Any]:
    if not path.exists():
        return {}
    try:
        with path.open("r", encoding="utf-8") as file:
            value = json.load(file)
    except (OSError, json.JSONDecodeError):
        return {}
    return value if isinstance(value, dict) else {}


def coerce_value(field: dict[str, Any], value: Any, fallback: Any) -> Any:
    if field["type"] == "boolean":
        return bool(value)
    if field["type"] == "string":
        return "" if value is None else str(value)

    try:
        number = float(value)
    except (TypeError, ValueError):
        return fallback
    if not math.isfinite(number):
        return fallback
    if "min" in field:
        number = max(float(field["min"]), number)
    if "max" in field:
        number = min(float(field["max"]), number)
    return round(number, MAX_NUMERIC_DECIMALS)


def coerce_profile(source: dict[str, Any], fallback: dict[str, Any]) -> dict[str, Any]:
    profile = dict(source)
    for key, field in FIELDS_BY_KEY.items():
        if key in source:
            profile[key] = coerce_value(field, source[key], fallback.get(key))
        elif key in fallback:
            profile[key] = fallback[key]
    return profile


def ordered_profile(profile: dict[str, Any]) -> dict[str, Any]:
    ordered: dict[str, Any] = {}
    for section in SCHEMA:
        for field in section["fields"]:
            key = field["key"]
            if key in profile:
                ordered[key] = profile[key]
            lock_key = LOCK_KEYS.get(key)
            if lock_key in profile:
                ordered[lock_key] = profile[lock_key]
    if PROFILE_COMMENT_KEY in profile:
        ordered[PROFILE_COMMENT_KEY] = profile[PROFILE_COMMENT_KEY]
    for key in sorted(profile):
        if key not in ordered:
            ordered[key] = profile[key]
    return ordered


def write_json_atomic(path: Path, value: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with NamedTemporaryFile("w", encoding="utf-8", dir=path.parent, delete=False) as temp_file:
        json.dump(ordered_profile(value), temp_file, indent=2)
        temp_file.write("\n")
        temp_name = temp_file.name
    os.replace(temp_name, path)


def normalize_profile_name(value: Any, fallback: str = "current") -> str:
    profile_name = str(value).strip().lower()
    if profile_name == "default":
        return "preset"
    if profile_name in PROFILE_FILES:
        return profile_name
    return fallback if fallback in PROFILE_FILES else "current"


def safe_saved_profile_name(value: Any) -> str:
    raw_name = str(value or "").strip()
    if not raw_name:
        raise ValueError("Expected a profile name.")
    safe_name = re.sub(r"[^A-Za-z0-9._-]+", "_", raw_name).strip("._-")
    if safe_name.lower().endswith(".json"):
        safe_name = safe_name[:-5].strip("._-")
    if not safe_name:
        raise ValueError("Profile path must contain letters, digits, dots, dashes, or underscores.")
    return safe_name[:80]


def safe_saved_profile_reference(value: Any) -> str:
    raw_name = str(value or "").strip().replace("\\", "/")
    if not raw_name:
        raise ValueError("Expected a profile path or name.")
    if Path(raw_name).is_absolute():
        raise ValueError("Saved profile path must be relative.")
    if raw_name.lower().endswith(".json"):
        raw_name = raw_name[:-5].strip()

    raw_parts = [part.strip() for part in raw_name.split("/") if part.strip()]
    if raw_parts and raw_parts[0].lower() == "assets":
        raw_parts = raw_parts[1:]
    if raw_parts and raw_parts[0].lower() == SAVED_PROFILES_DIR:
        raw_parts = raw_parts[1:]
    if not raw_parts:
        raise ValueError("Expected a profile path or name below saved_profiles.")
    if any(part in {".", ".."} for part in raw_parts):
        raise ValueError("Saved profile path cannot contain . or .. segments.")

    safe_parts = [safe_saved_profile_name(part) for part in raw_parts]
    return "/".join(safe_parts)


def read_required_json_object(path: Path) -> dict[str, Any]:
    try:
        with path.open("r", encoding="utf-8") as file:
            value = json.load(file)
    except FileNotFoundError:
        raise
    except (OSError, json.JSONDecodeError) as error:
        raise ValueError(f"Cannot read saved profile: {path.name}") from error
    if not isinstance(value, dict):
        raise ValueError(f"Saved profile must be a JSON object: {path.name}")
    return value


class ConfigStore:
    def __init__(self, assets_dir: Path) -> None:
        self.assets_dir = assets_dir
        self.repo_root = assets_dir.parent
        self.assets_dir.mkdir(parents=True, exist_ok=True)

    def profile_path(self, profile_name: str) -> Path:
        return self.assets_dir / PROFILE_FILES[profile_name]

    def runtime_state_path(self) -> Path:
        return self.assets_dir / RUNTIME_STATE_FILE

    def saved_profiles_dir(self) -> Path:
        return self.assets_dir / SAVED_PROFILES_DIR

    def hfc_images_dir(self) -> Path:
        return self.assets_dir / HFC_IMAGES_DIR

    def saved_profile_path(self, requested_name: Any) -> tuple[str, Path]:
        safe_name = safe_saved_profile_reference(requested_name)
        return safe_name, self.saved_profiles_dir() / f"{safe_name}.json"

    def saved_profile_name_for_path(self, path: Path) -> str | None:
        try:
            relative_path = path.resolve().relative_to(self.saved_profiles_dir().resolve())
        except (OSError, ValueError):
            return None
        if relative_path.suffix.lower() not in PROFILE_EXTENSIONS:
            return None
        return relative_path.with_suffix("").as_posix()

    def hfc_relative_image_path(self, path: Path) -> str | None:
        try:
            relative_path = path.expanduser().resolve().relative_to(self.hfc_images_dir().resolve())
        except (OSError, ValueError):
            return None
        return relative_path.as_posix()

    def normalize_image_profile_path(self, value: Any) -> str:
        raw_path = str(value or "").strip().replace("\\", "/")
        if not raw_path:
            return ""

        lowered_path = raw_path.lower()
        assets_hfc_prefix = f"assets/{HFC_IMAGES_DIR}/"
        hfc_prefix = f"{HFC_IMAGES_DIR}/"
        if lowered_path == f"assets/{HFC_IMAGES_DIR}" or lowered_path == HFC_IMAGES_DIR:
            return ""
        if lowered_path.startswith(assets_hfc_prefix):
            return raw_path[len(assets_hfc_prefix):].strip("/")
        if lowered_path.startswith(hfc_prefix):
            return raw_path[len(hfc_prefix):].strip("/")

        path = Path(raw_path).expanduser()
        if path.is_absolute():
            relative_path = self.hfc_relative_image_path(path)
            if relative_path is not None:
                return relative_path
        return raw_path

    def normalize_profile_image_path(self, profile: dict[str, Any]) -> dict[str, Any]:
        normalized = dict(profile)
        normalized["fisheye_image_path"] = self.normalize_image_profile_path(
            normalized.get("fisheye_image_path", "")
        )
        return normalized

    def resolve_image_path(self, raw_path: str | None, default: Path | None = None) -> Path:
        raw_text = str(raw_path or "").strip().replace("\\", "/")
        if raw_text:
            raw_path_obj = Path(raw_text).expanduser()
            lowered_path = raw_text.lower()
            if raw_path_obj.is_absolute():
                candidate = raw_path_obj
            elif lowered_path == "assets":
                candidate = self.assets_dir
            elif lowered_path.startswith("assets/"):
                candidate = self.repo_root / raw_text
            elif lowered_path == HFC_IMAGES_DIR or lowered_path.startswith(f"{HFC_IMAGES_DIR}/"):
                candidate = self.assets_dir / raw_text
            elif lowered_path == "testing-images" or lowered_path.startswith("testing-images/"):
                candidate = self.assets_dir / raw_text
            else:
                candidate = self.hfc_images_dir() / raw_text
        elif default is not None:
            candidate = default
        else:
            candidate = self.hfc_images_dir()
            if not candidate.exists():
                candidate = self.assets_dir / "testing-images"
            if not candidate.exists():
                candidate = self.assets_dir
        return candidate.resolve()

    def image_profile_path_for_path(self, path: Path) -> str:
        relative_path = self.hfc_relative_image_path(path)
        if relative_path is not None:
            return relative_path
        return str(path.resolve())

    def read_profiles(self) -> dict[str, dict[str, Any]]:
        legacy_preset = read_json_file(self.assets_dir / "default_state.json")
        preset = self.normalize_profile_image_path(coerce_profile(
            read_json_file(self.profile_path("preset")) or legacy_preset,
            default_profile(self.assets_dir),
        ))
        current = self.normalize_profile_image_path(coerce_profile(
            read_json_file(self.profile_path("current")),
            preset,
        ))
        return {"current": current, "preset": preset}

    def read_runtime_state(self) -> dict[str, Any]:
        return read_json_file(self.runtime_state_path())

    def read_active_profile(self) -> str:
        runtime_state = self.read_runtime_state()
        return normalize_profile_name(runtime_state.get(ACTIVE_PROFILE_KEY), "current")

    def read_current_profile_name(self) -> str:
        raw_name = str(self.read_runtime_state().get(CURRENT_PROFILE_NAME_KEY, "Current")).strip()
        return raw_name or "Current"

    def read_image_panel_key_changes(self) -> str:
        text = self.read_runtime_state().get(IMAGE_PANEL_KEY_CHANGES_KEY, DEFAULT_IMAGE_PANEL_KEY_CHANGES)
        return str(text if text is not None else "")

    def update_image_panel_key_changes(self, text: Any) -> dict[str, str]:
        runtime_state = self.read_runtime_state()
        runtime_state[IMAGE_PANEL_KEY_CHANGES_KEY] = str(text if text is not None else "")
        if ACTIVE_PROFILE_KEY not in runtime_state:
            runtime_state[ACTIVE_PROFILE_KEY] = "current"
        if CURRENT_PROFILE_NAME_KEY not in runtime_state:
            runtime_state[CURRENT_PROFILE_NAME_KEY] = "Current"
        write_json_atomic(self.runtime_state_path(), runtime_state)
        return {"imagePanelKeyChanges": runtime_state[IMAGE_PANEL_KEY_CHANGES_KEY]}

    def update_runtime_state(
        self,
        profile_name: Any | None = None,
        current_profile_name: Any | None = None,
    ) -> dict[str, str]:
        runtime_state = self.read_runtime_state()
        if profile_name is not None:
            runtime_state[ACTIVE_PROFILE_KEY] = normalize_profile_name(
                profile_name,
                runtime_state.get(ACTIVE_PROFILE_KEY, "current"),
            )
        if current_profile_name is not None:
            runtime_state[CURRENT_PROFILE_NAME_KEY] = str(current_profile_name).strip() or "Current"
        if ACTIVE_PROFILE_KEY not in runtime_state:
            runtime_state[ACTIVE_PROFILE_KEY] = "current"
        if CURRENT_PROFILE_NAME_KEY not in runtime_state:
            runtime_state[CURRENT_PROFILE_NAME_KEY] = "Current"
        runtime_state[ACTIVE_PROFILE_KEY] = normalize_profile_name(
            runtime_state.get(ACTIVE_PROFILE_KEY),
            "current",
        )
        write_json_atomic(self.runtime_state_path(), runtime_state)
        return {
            "activeProfile": normalize_profile_name(runtime_state.get(ACTIVE_PROFILE_KEY), "current"),
            "currentProfileName": str(runtime_state.get(CURRENT_PROFILE_NAME_KEY, "Current")).strip()
            or "Current",
        }

    def update_active_profile(self, profile_name: Any) -> dict[str, str]:
        return self.update_runtime_state(profile_name=profile_name)

    def update_profile(self, profile_name: str, patch: dict[str, Any]) -> dict[str, Any]:
        if profile_name not in PROFILE_FILES:
            raise KeyError(profile_name)
        profiles = self.read_profiles()
        fallback = profiles["preset"] if profile_name == "current" else default_profile(self.assets_dir)
        existing = dict(profiles[profile_name])
        for key, value in patch.items():
            if key == PROFILE_COMMENT_KEY:
                existing[key] = "" if value is None else str(value)
                continue
            field = FIELDS_BY_KEY.get(key)
            if field is None:
                continue
            existing[key] = coerce_value(field, value, fallback.get(key))
        existing = self.normalize_profile_image_path(coerce_profile(existing, fallback))
        write_json_atomic(self.profile_path(profile_name), existing)
        return existing

    def save_current_as(self, requested_name: Any) -> dict[str, str]:
        safe_name, target_path = self.saved_profile_path(requested_name)
        current = self.read_profiles()["current"]
        write_json_atomic(target_path, current)
        runtime_state = self.update_runtime_state(current_profile_name=safe_name)
        return {"name": safe_name, "path": str(target_path.resolve()), **runtime_state}

    def list_saved_profiles(self) -> dict[str, Any]:
        saved_dir = self.saved_profiles_dir()
        if not saved_dir.is_dir():
            return {"directory": str(saved_dir.resolve()), "profiles": []}
        profiles: list[dict[str, Any]] = []
        for path in sorted(
            saved_dir.rglob("*.json"),
            key=lambda candidate: candidate.relative_to(saved_dir).as_posix().casefold(),
        ):
            if not path.is_file():
                continue
            try:
                stat = path.stat()
            except OSError:
                continue
            profile_name = path.relative_to(saved_dir).with_suffix("").as_posix()
            profiles.append({
                "name": profile_name,
                "path": str(path.resolve()),
                "modified": stat.st_mtime,
            })
        return {"directory": str(saved_dir.resolve()), "profiles": profiles}

    def load_saved_profile_as_current(self, requested_name: Any) -> dict[str, Any]:
        safe_name, source_path = self.saved_profile_path(requested_name)
        if not source_path.is_file():
            raise FileNotFoundError(f"Saved profile does not exist: {safe_name}")
        profiles = self.read_profiles()
        loaded_profile = self.normalize_profile_image_path(
            coerce_profile(read_required_json_object(source_path), profiles["preset"])
        )
        write_json_atomic(self.profile_path("current"), loaded_profile)
        runtime_state = self.update_runtime_state(profile_name="current", current_profile_name=safe_name)
        return {"name": safe_name, "profile": loaded_profile, **runtime_state}

    def delete_saved_profile(self, requested_name: Any) -> dict[str, Any]:
        safe_name, target_path = self.saved_profile_path(requested_name)
        if not target_path.is_file():
            raise FileNotFoundError(f"Saved profile does not exist: {safe_name}")
        target_path.unlink()
        return {
            "name": safe_name,
            "deleted": True,
            "directory": str(self.saved_profiles_dir().resolve()),
        }

    def rename_saved_profile(self, requested_name: Any, requested_new_name: Any) -> dict[str, Any]:
        old_name, source_path = self.saved_profile_path(requested_name)
        new_name, target_path = self.saved_profile_path(requested_new_name)
        if old_name == new_name:
            raise ValueError("Choose a different profile name before renaming.")
        if not source_path.is_file():
            raise FileNotFoundError(f"Saved profile does not exist: {old_name}")
        if target_path.exists():
            raise FileExistsError(f"Saved profile already exists: {new_name}")

        target_path.parent.mkdir(parents=True, exist_ok=True)
        source_path.rename(target_path)
        runtime_state = {
            "activeProfile": self.read_active_profile(),
            "currentProfileName": self.read_current_profile_name(),
        }
        if runtime_state["currentProfileName"] == old_name:
            runtime_state = self.update_runtime_state(current_profile_name=new_name)
        return {
            "name": new_name,
            "oldName": old_name,
            "newName": new_name,
            "path": str(target_path.resolve()),
            "renamed": True,
            **runtime_state,
        }

    def explorer_roots(self) -> list[dict[str, str]]:
        candidates = [
            ("HFC Images", self.hfc_images_dir()),
            ("Testing Images", self.assets_dir / "testing-images"),
            ("Assets", self.assets_dir),
            ("Repository", self.repo_root),
            ("Home", Path.home()),
        ]
        roots: list[dict[str, str]] = []
        seen: set[str] = set()
        for label, path in candidates:
            try:
                resolved = path.expanduser().resolve()
            except OSError:
                continue
            if not resolved.is_dir():
                continue
            resolved_text = str(resolved)
            if resolved_text in seen:
                continue
            seen.add(resolved_text)
            roots.append({"label": label, "path": resolved_text})
        return roots

    def profile_file_roots(self) -> list[dict[str, str]]:
        candidates = [
            ("Saved Profiles", self.saved_profiles_dir()),
            ("Assets", self.assets_dir),
            ("Repository", self.repo_root),
            ("Home", Path.home()),
        ]
        roots: list[dict[str, str]] = []
        seen: set[str] = set()
        for label, path in candidates:
            try:
                resolved = path.expanduser().resolve()
            except OSError:
                continue
            if not resolved.is_dir():
                continue
            resolved_text = str(resolved)
            if resolved_text in seen:
                continue
            seen.add(resolved_text)
            roots.append({"label": label, "path": resolved_text})
        return roots

    def resolve_user_path(self, raw_path: str | None, default: Path | None = None) -> Path:
        if raw_path:
            candidate = Path(raw_path).expanduser()
            if not candidate.is_absolute():
                candidate = self.repo_root / candidate
        elif default is not None:
            candidate = default
        else:
            candidate = self.assets_dir / "testing-images"
            if not candidate.exists():
                candidate = self.assets_dir
        return candidate.resolve()

    def list_image_directory(self, raw_dir: str | None) -> dict[str, Any]:
        directory = self.resolve_image_path(raw_dir)
        if directory.is_file():
            directory = directory.parent
        if not directory.exists():
            raise FileNotFoundError(f"Directory does not exist: {directory}")
        if not directory.is_dir():
            raise NotADirectoryError(f"Not a directory: {directory}")

        entries: list[dict[str, Any]] = []
        for child in directory.iterdir():
            if child.name.startswith("."):
                continue
            try:
                if child.is_dir():
                    entries.append({
                        "type": "directory",
                        "kind": "dir",
                        "name": child.name,
                        "path": str(child.resolve()),
                        "size": 0,
                    })
                elif child.is_file() and child.suffix.lower() in IMAGE_EXTENSIONS:
                    profile_path = self.image_profile_path_for_path(child)
                    entries.append({
                        "type": "image",
                        "kind": "hfc" if self.hfc_relative_image_path(child) is not None else "image",
                        "name": child.name,
                        "path": str(child.resolve()),
                        "profilePath": profile_path,
                        "size": child.stat().st_size,
                    })
            except OSError:
                continue

        entries.sort(key=lambda entry: (entry["type"] != "directory", entry["name"].casefold()))
        parent = directory.parent if directory.parent != directory else None
        return {
            "directory": str(directory),
            "parent": str(parent) if parent is not None else "",
            "roots": self.explorer_roots(),
            "entries": entries,
        }

    def list_profile_file_directory(self, raw_dir: str | None) -> dict[str, Any]:
        default_directory = self.saved_profiles_dir()
        if not default_directory.is_dir():
            default_directory = self.assets_dir
        directory = self.resolve_user_path(raw_dir, default=default_directory)
        if directory.is_file():
            directory = directory.parent
        if not directory.exists():
            raise FileNotFoundError(f"Directory does not exist: {directory}")
        if not directory.is_dir():
            raise NotADirectoryError(f"Not a directory: {directory}")

        entries: list[dict[str, Any]] = []
        for child in directory.iterdir():
            if child.name.startswith("."):
                continue
            try:
                if child.is_dir():
                    entries.append({
                        "type": "directory",
                        "name": child.name,
                        "path": str(child.resolve()),
                        "size": 0,
                    })
                elif child.is_file() and child.suffix.lower() in PROFILE_EXTENSIONS:
                    entries.append({
                        "type": "profile",
                        "name": child.name,
                        "path": str(child.resolve()),
                        "size": child.stat().st_size,
                    })
            except OSError:
                continue

        entries.sort(key=lambda entry: (entry["type"] != "directory", entry["name"].casefold()))
        parent = directory.parent if directory.parent != directory else None
        return {
            "directory": str(directory),
            "parent": str(parent) if parent is not None else "",
            "roots": self.profile_file_roots(),
            "entries": entries,
        }

    def open_profile_file_as_current(self, raw_path: str | None) -> dict[str, Any]:
        path = self.resolve_user_path(raw_path, default=self.saved_profiles_dir())
        if path.suffix.lower() not in PROFILE_EXTENSIONS:
            raise ValueError(f"Unsupported profile extension: {path.suffix}")
        if not path.is_file():
            raise FileNotFoundError(f"Profile file does not exist: {path}")

        profiles = self.read_profiles()
        loaded_profile = self.normalize_profile_image_path(
            coerce_profile(read_required_json_object(path), profiles["preset"])
        )
        write_json_atomic(self.profile_path("current"), loaded_profile)
        profile_name = self.saved_profile_name_for_path(path) or safe_saved_profile_name(path.stem)
        runtime_state = self.update_runtime_state(profile_name="current", current_profile_name=profile_name)
        return {
            "name": profile_name,
            "path": str(path.resolve()),
            "profile": loaded_profile,
            **runtime_state,
        }

    def hfc_image_path(self, raw_path: str | None, must_exist: bool = True) -> tuple[str, Path]:
        path = self.resolve_image_path(raw_path)
        relative_path = self.hfc_relative_image_path(path)
        if relative_path is None:
            raise ValueError("HFC image path must be below assets/hfc-images.")
        if not relative_path:
            raise ValueError("Expected an HFC image path.")
        parts = Path(relative_path).parts
        if any(part in {"", ".", ".."} for part in parts):
            raise ValueError("HFC image path cannot contain . or .. segments.")
        if path.suffix.lower() not in IMAGE_EXTENSIONS:
            raise ValueError(f"Unsupported image extension: {path.suffix}")
        if must_exist and not path.is_file():
            raise FileNotFoundError(f"HFC image does not exist: {relative_path}")
        return relative_path, path

    def hfc_image_target_path(self, raw_path: str | None, source_path: Path) -> tuple[str, Path]:
        raw_text = str(raw_path or "").strip().replace("\\", "/")
        if not raw_text:
            raise ValueError("Expected an HFC image path.")
        if not Path(raw_text).suffix:
            raw_text = f"{raw_text.rstrip('/')}{source_path.suffix.lower()}"
        return self.hfc_image_path(raw_text, must_exist=False)

    def source_image_path(self, raw_path: str | None) -> Path:
        path = self.resolve_image_path(raw_path)
        if path.suffix.lower() not in IMAGE_EXTENSIONS:
            raise ValueError(f"Unsupported image extension: {path.suffix}")
        if not path.is_file():
            raise FileNotFoundError(f"Image does not exist: {path}")
        return path

    def save_hfc_image(
        self,
        raw_source_path: str | None,
        raw_target_path: str | None,
        overwrite: bool = False,
    ) -> dict[str, Any]:
        source_path = self.source_image_path(raw_source_path)
        profile_path, target_path = self.hfc_image_target_path(raw_target_path, source_path)
        same_file = source_path.resolve() == target_path.resolve()
        overwritten = target_path.exists() and not same_file
        if overwritten and not overwrite:
            raise FileExistsError(f"HFC image already exists: {profile_path}")

        if not same_file:
            target_path.parent.mkdir(parents=True, exist_ok=True)
            with NamedTemporaryFile("wb", dir=target_path.parent, delete=False) as temp_file:
                temp_path = Path(temp_file.name)
            try:
                shutil.copy2(source_path, temp_path)
                os.replace(temp_path, target_path)
            finally:
                try:
                    if temp_path.exists():
                        temp_path.unlink()
                except OSError:
                    pass

        return {
            "sourcePath": str(source_path.resolve()),
            "profilePath": profile_path,
            "path": str(target_path.resolve()),
            "saved": True,
            "overwritten": overwritten,
        }

    def rename_hfc_image(self, raw_path: str | None, raw_new_path: str | None) -> dict[str, Any]:
        old_profile_path, source_path = self.hfc_image_path(raw_path, must_exist=True)
        new_profile_path, target_path = self.hfc_image_path(raw_new_path, must_exist=False)
        if old_profile_path == new_profile_path:
            raise ValueError("Choose a different HFC image path before renaming.")
        if target_path.exists():
            raise FileExistsError(f"HFC image already exists: {new_profile_path}")

        target_path.parent.mkdir(parents=True, exist_ok=True)
        source_path.rename(target_path)
        return {
            "oldProfilePath": old_profile_path,
            "oldPath": str(source_path.resolve()),
            "profilePath": new_profile_path,
            "path": str(target_path.resolve()),
            "renamed": True,
        }

    def delete_hfc_image(self, raw_path: str | None) -> dict[str, Any]:
        profile_path, path = self.hfc_image_path(raw_path, must_exist=True)
        path.unlink()
        return {
            "profilePath": profile_path,
            "path": str(path.resolve()),
            "deleted": True,
        }

    def read_image_file(self, raw_path: str | None) -> tuple[bytes, str]:
        path = self.resolve_image_path(raw_path)
        if path.suffix.lower() not in IMAGE_EXTENSIONS:
            raise ValueError(f"Unsupported image extension: {path.suffix}")
        if not path.is_file():
            raise FileNotFoundError(f"Image does not exist: {path}")
        content_type = IMAGE_MIME_TYPES.get(path.suffix.lower())
        if content_type is None:
            content_type = mimetypes.guess_type(str(path))[0] or "application/octet-stream"
        return path.read_bytes(), content_type


class ConfigRequestHandler(BaseHTTPRequestHandler):
    store: ConfigStore

    def do_GET(self) -> None:
        parsed = urlparse(self.path)
        path = parsed.path
        if path in {"/", "/index.html"}:
            self.send_bytes(INDEX_HTML.encode("utf-8"), "text/html; charset=utf-8")
            return
        if path == "/api/state":
            self.send_json({
                "schema": SCHEMA,
                "lockKeys": LOCK_KEYS,
                "profiles": self.store.read_profiles(),
                "activeProfile": self.store.read_active_profile(),
                "currentProfileName": self.store.read_current_profile_name(),
                "imagePanelKeyChanges": self.store.read_image_panel_key_changes(),
            })
            return
        if path == "/api/schema":
            self.send_json({"schema": SCHEMA, "lockKeys": LOCK_KEYS})
            return
        if path == "/api/active-profile":
            self.send_json({
                "activeProfile": self.store.read_active_profile(),
                "currentProfileName": self.store.read_current_profile_name(),
            })
            return
        if path == "/api/saved-profiles":
            self.send_json(self.store.list_saved_profiles())
            return
        if path == "/api/files":
            query = parse_qs(parsed.query)
            directory = query.get("dir", [""])[0]
            try:
                self.send_json(self.store.list_image_directory(directory))
            except (FileNotFoundError, NotADirectoryError, OSError) as error:
                self.send_json_error(str(error), HTTPStatus.BAD_REQUEST)
            return
        if path == "/api/profile-files":
            query = parse_qs(parsed.query)
            directory = query.get("dir", [""])[0]
            try:
                self.send_json(self.store.list_profile_file_directory(directory))
            except (FileNotFoundError, NotADirectoryError, OSError) as error:
                self.send_json_error(str(error), HTTPStatus.BAD_REQUEST)
            return
        if path == "/api/image":
            query = parse_qs(parsed.query)
            image_path = query.get("path", [""])[0]
            try:
                body, content_type = self.store.read_image_file(image_path)
            except (FileNotFoundError, ValueError, OSError) as error:
                self.send_json_error(str(error), HTTPStatus.NOT_FOUND)
                return
            self.send_bytes(body, content_type)
            return
        if path == "/favicon.ico":
            self.send_response(HTTPStatus.NO_CONTENT)
            self.end_headers()
            return
        self.send_error(HTTPStatus.NOT_FOUND)

    def do_POST(self) -> None:
        path = urlparse(self.path).path
        parts = path.strip("/").split("/")
        try:
            length = int(self.headers.get("Content-Length", "0"))
            payload = json.loads(self.rfile.read(length).decode("utf-8") or "{}")
        except (ValueError, json.JSONDecodeError):
            self.send_error(HTTPStatus.BAD_REQUEST, "Expected a JSON object.")
            return
        if not isinstance(payload, dict):
            self.send_error(HTTPStatus.BAD_REQUEST, "Expected a JSON object.")
            return

        if len(parts) == 3 and parts[:2] == ["api", "state"] and parts[2] in PROFILE_FILES:
            profile = self.store.update_profile(parts[2], payload)
            self.send_json({
                "profile": profile,
                "activeProfile": self.store.read_active_profile(),
                "currentProfileName": self.store.read_current_profile_name(),
            })
            return

        if parts == ["api", "active-profile"]:
            runtime_state = self.store.update_active_profile(
                payload.get("activeProfile", payload.get(ACTIVE_PROFILE_KEY))
            )
            self.send_json(runtime_state)
            return

        if parts == ["api", "image-panel-key-changes"]:
            self.send_json(self.store.update_image_panel_key_changes(payload.get("text", "")))
            return

        if parts == ["api", "saved-profiles", "current"]:
            try:
                self.send_json(self.store.save_current_as(payload.get("name")))
            except ValueError as error:
                self.send_json_error(str(error), HTTPStatus.BAD_REQUEST)
            return

        if parts == ["api", "saved-profiles", "current", "load"]:
            try:
                self.send_json(self.store.load_saved_profile_as_current(payload.get("name")))
            except ValueError as error:
                self.send_json_error(str(error), HTTPStatus.BAD_REQUEST)
            except FileNotFoundError as error:
                self.send_json_error(str(error), HTTPStatus.NOT_FOUND)
            return

        if parts == ["api", "profile-files", "current", "open"]:
            try:
                self.send_json(self.store.open_profile_file_as_current(payload.get("path")))
            except ValueError as error:
                self.send_json_error(str(error), HTTPStatus.BAD_REQUEST)
            except FileNotFoundError as error:
                self.send_json_error(str(error), HTTPStatus.NOT_FOUND)
            except OSError as error:
                self.send_json_error(str(error), HTTPStatus.INTERNAL_SERVER_ERROR)
            return

        if parts == ["api", "saved-profiles", "current", "delete"]:
            try:
                self.send_json(self.store.delete_saved_profile(payload.get("name")))
            except ValueError as error:
                self.send_json_error(str(error), HTTPStatus.BAD_REQUEST)
            except FileNotFoundError as error:
                self.send_json_error(str(error), HTTPStatus.NOT_FOUND)
            except OSError as error:
                self.send_json_error(str(error), HTTPStatus.INTERNAL_SERVER_ERROR)
            return

        if parts == ["api", "saved-profiles", "current", "rename"]:
            try:
                self.send_json(self.store.rename_saved_profile(payload.get("name"), payload.get("newName")))
            except ValueError as error:
                self.send_json_error(str(error), HTTPStatus.BAD_REQUEST)
            except FileNotFoundError as error:
                self.send_json_error(str(error), HTTPStatus.NOT_FOUND)
            except FileExistsError as error:
                self.send_json_error(str(error), HTTPStatus.CONFLICT)
            except OSError as error:
                self.send_json_error(str(error), HTTPStatus.INTERNAL_SERVER_ERROR)
            return

        if parts == ["api", "hfc-images", "save"]:
            try:
                self.send_json(self.store.save_hfc_image(
                    payload.get("sourcePath"),
                    payload.get("path"),
                    bool(payload.get("overwrite", False)),
                ))
            except ValueError as error:
                self.send_json_error(str(error), HTTPStatus.BAD_REQUEST)
            except FileNotFoundError as error:
                self.send_json_error(str(error), HTTPStatus.NOT_FOUND)
            except FileExistsError as error:
                self.send_json_error(str(error), HTTPStatus.CONFLICT)
            except OSError as error:
                self.send_json_error(str(error), HTTPStatus.INTERNAL_SERVER_ERROR)
            return

        if parts == ["api", "hfc-images", "rename"]:
            try:
                self.send_json(self.store.rename_hfc_image(payload.get("path"), payload.get("newPath")))
            except ValueError as error:
                self.send_json_error(str(error), HTTPStatus.BAD_REQUEST)
            except FileNotFoundError as error:
                self.send_json_error(str(error), HTTPStatus.NOT_FOUND)
            except FileExistsError as error:
                self.send_json_error(str(error), HTTPStatus.CONFLICT)
            except OSError as error:
                self.send_json_error(str(error), HTTPStatus.INTERNAL_SERVER_ERROR)
            return

        if parts == ["api", "hfc-images", "delete"]:
            try:
                self.send_json(self.store.delete_hfc_image(payload.get("path")))
            except ValueError as error:
                self.send_json_error(str(error), HTTPStatus.BAD_REQUEST)
            except FileNotFoundError as error:
                self.send_json_error(str(error), HTTPStatus.NOT_FOUND)
            except OSError as error:
                self.send_json_error(str(error), HTTPStatus.INTERNAL_SERVER_ERROR)
            return

        self.send_error(HTTPStatus.NOT_FOUND)

    def do_OPTIONS(self) -> None:
        self.send_response(HTTPStatus.NO_CONTENT)
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Headers", "Content-Type")
        self.send_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
        self.end_headers()

    def log_message(self, format: str, *args: Any) -> None:
        print(f"{self.address_string()} - {format % args}")

    def send_json(self, value: dict[str, Any]) -> None:
        body = json.dumps(value).encode("utf-8")
        self.send_bytes(body, "application/json")

    def send_json_error(self, message: str, status: HTTPStatus) -> None:
        body = json.dumps({"error": message}).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.send_header("Access-Control-Allow-Origin", "*")
        self.end_headers()
        try:
            self.wfile.write(body)
        except (BrokenPipeError, ConnectionResetError):
            pass

    def send_bytes(self, body: bytes, content_type: str) -> None:
        self.send_response(HTTPStatus.OK)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.send_header("Access-Control-Allow-Origin", "*")
        self.end_headers()
        self.wfile.write(body)


def make_handler(store: ConfigStore) -> type[ConfigRequestHandler]:
    class Handler(ConfigRequestHandler):
        pass

    Handler.store = store
    return Handler


def parse_args() -> argparse.Namespace:
    repo_root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description="Serve a browser UI for Pixel To Ray Qt profiles.")
    parser.add_argument("--host", default="127.0.0.1", help="Bind address. Default: 127.0.0.1")
    parser.add_argument("--port", type=int, default=8765, help="Bind port. Default: 8765")
    parser.add_argument(
        "--assets-dir",
        type=Path,
        default=repo_root / "assets",
        help="Directory containing current_state.json and preset_state.json.",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    store = ConfigStore(args.assets_dir.resolve())
    handler = make_handler(store)

    server: ThreadingHTTPServer | None = None
    last_error: OSError | None = None
    candidate_ports = [args.port] if args.port == 0 else list(range(args.port, args.port + 11))
    for candidate_port in candidate_ports:
        try:
            server = ThreadingHTTPServer((args.host, candidate_port), handler)
            break
        except OSError as error:
            if error.errno != errno.EADDRINUSE:
                raise
            last_error = error

    if server is None:
        if last_error is not None:
            raise SystemExit(
                f"Could not bind to {args.host}:{args.port} because the address is already in use."
            ) from last_error
        raise SystemExit(f"Could not start server on {args.host}:{args.port}.")

    bound_host, bound_port = server.server_address[:2]
    if bound_port != args.port:
        print(f"Port {args.port} is busy; using http://{bound_host}:{bound_port}/")
    else:
        print(f"Serving Pixel To Ray browser config at http://{bound_host}:{bound_port}/")
    print(f"Using state profiles in {store.assets_dir}")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nStopping server.")
    finally:
        server.server_close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
