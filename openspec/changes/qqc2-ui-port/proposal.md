# Proposal: Qt Quick Controls UI Port (de-Kirigami)

## Why

The picker, hold HUD, and settings window are built on Kirigami + kirigami-addons FormCard (~80 delegates, `Kirigami.Theme` at 63 sites, `Kirigami.Units` at 22, `ListItemDragHandle`, `ShadowedRectangle`, `FormCardPage` as every settings page's root). Kirigami keeps the entire UI inside the KDE framework stack — the single largest remaining blocker to shipping Lane on Windows and macOS, and a guarantee the app would look like a KDE app everywhere else forever.

Qt Quick Controls 2 in Fusion style plus the platform theme (colors/fonts/icons — which on Plasma comes from KDE's own platformtheme plugin, no QML runtime) renders acceptably on Plasma and natively on Windows/macOS — one QML codebase instead of per-OS UI trees. (`qqc2-desktop-style` itself imports Kirigami, so it cannot be the style: it would violate this change's no-KDE-QML-runtime contract. Verified: missing primary style also means no usable fallback — programmatic setStyle must go.)

## What Changes

- **New `ui/` QML module** (`src/qml/ui/`, URI `app.lane.ui`, separate `qt_add_qml_module` static target): `ScrollableFormPage` (FormCardPage replacement — scroll flickable, centered max card width, ensure-visible-on-focus), `FormCard`, `FormHeader`, `FormDelegateSeparator`, `SwitchRow`, `ComboRow`, `ButtonRow`, `TextRow`, `TextFieldRow`, `SpinBoxRow`, `ThemedShadow` (`QtQuick.Effects.MultiEffect`, Qt 6.5 — under the 6.6 floor), `DragHandle`, `Theme` singleton (metrics + semantic colors).
- **All 8 QML files ported**: `Picker.qml`, `Hold.qml`, `Settings.qml` (keep its existing QQC2 ApplicationWindow/RowLayout shell — only replace Kirigami colors/icons), `pages/{Overview,Targets,Rules,Preferences,About}Page.qml` (About lands via bare-qt6-port? No — Overview already covers metadata/license; no About page).
- **`Kirigami.Theme`** → attached `palette` colors + `ui.Theme` semantic colors (positive/negative/neutral text — QPalette lacks them); **`Kirigami.Units`** → `ui.Theme` metrics that still scale with font size where spacing is font-relative; **`Kirigami.Icon`** → `image://laneicon/` `QQuickImageProvider` (C++ — `QIcon::fromTheme` pixmap; replaces IconImage which is private `QtQuick.Controls.impl`); **`ListItemDragHandle`** → `DragHandle` preserving the captured-id + disabled-while-filtering invariants; **`FormCardPage`** → `ScrollableFormPage`; **`ShadowedRectangle`** → `Rectangle` + `MultiEffect`.
- **Style**: delete `QQuickStyle::setStyle("org.kde.desktop")` in `main.cpp`; `qtquickcontrols2.conf` ships `Style=Fusion` — Plasma gets its platform colors via the KDE platformtheme plugin, Windows/macOS get Fusion/Fluent via the same conf.
- **Small C++ surface**: `ThemeIconProvider` registered in `Controller::ensureEngine` (the only C++ change — supersedes the earlier no-C++ claim).
- **Build**: `ecm_find_qmlmodule(org.kde.kirigami...)` checks removed; `qt_add_qml_module` gains the `ui` target.

## Capabilities

### Modified Capabilities
- `lane`: Picker overlay and Hold HUD requirements keep all existing normative text/scenarios verbatim, re-pinning "Wayland layer-shell" and "exclusive keyboard", plus the QQC2 rendering clause. New ADDED requirements pin settings-window behavior (drag reorder; private windows not orderable) and platform styling (Fusion + platform theme, no KDE QML runtime).

## Impact

- `src/qml/` — all files rewritten; `src/qml/ui/` added (~12 new files); `src/app/ThemeIconProvider.*` + `ensureEngine` registration.
- `CMakeLists.txt`, `src/CMakeLists.txt`, `packaging/PKGBUILD`, `CONTRIBUTING.md`, `CLAUDE.md`, `README.md`, `.github/workflows/ci.yml` — Kirigami deps dropped (lands after bare-qt6-port's own edits; apply order matters).
- No config, model, or API contract changes; C++ delta is one image provider + one setStyle deletion.
- **BREAKING** (runtime): Kirigami and kirigami-addons are no longer runtime requirements; the process loads zero KDE QML modules.
- Depends on `bare-qt6-port` merged first (shared CMake files; Overview/about strategy settled there).
