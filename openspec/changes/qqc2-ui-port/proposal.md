# Proposal: Qt Quick Controls UI Port (de-Kirigami)

## Why

The picker, hold HUD, and settings window are built on Kirigami + kirigami-addons FormCard (~80 delegates, `Kirigami.Theme` at 63 sites, `Kirigami.Units` at 22, `ListItemDragHandle`, `ShadowedRectangle`). Kirigami keeps the entire UI inside the KDE framework stack — the single largest remaining blocker to shipping Lane on Windows and macOS, and a guarantee the app would look like a KDE app everywhere else forever.

Qt Quick Controls 2 renders natively on Plasma via `qqc2-desktop-style` and natively on Windows/macOS via Fusion/Fluent styles — one QML codebase instead of per-OS UI trees.

## What Changes

- **New `ui/` QML module** (`src/qml/ui/`): local replacements for FormCard's grouped-card settings delegates — `FormCard` (grouped container), `FormHeader`, `FormDelegateSeparator`, `SwitchRow`, `ComboRow`, `ButtonRow`, `TextRow`, `TextFieldRow`, `SpinBoxRow` — plus `ThemedShadow` (drop-shadow card) and `DragHandle` (list drag reorder).
- **All 7 QML files ported**: `Picker.qml`, `Hold.qml`, `Settings.qml`, `pages/{Overview,Targets,Rules,Preferences,About}Page.qml` to Qt Quick Controls + the local `ui` module.
- **`Kirigami.Theme`** → application palette / style-attached colors; **`Kirigami.Units`** → fixed dp constants; **`Kirigami.Icon`** → `IconImage`/`Image` with a `QIcon::fromTheme` provider on Linux; **`ListItemDragHandle`** → hand-ported MouseArea-driven reorder honoring existing sibling rules (kind + incognito); **`ShadowedRectangle`** → `Rectangle` + `MultiEffect`/`DropShadow`.
- **Style policy**: `qqc2-desktop-style` on Linux when available (Plasma-native look), Fusion as universal fallback; Windows/macOS select Fusion/Fluent. No second QML tree.
- **Build**: `ecm_find_qmlmodule(org.kde.kirigami...)` checks and Kirigami imports removed; `qt_add_qml_module` gains the `ui` module files.

## Capabilities

### Modified Capabilities
- `lane`: picker overlay, hold HUD, and settings window are re-specified against Qt Quick Controls; all user-visible behavior (layout intent, keyboard model, drag reorder, hold progress) is unchanged.

### New Capabilities
- None — this is a rendering-layer port of existing behavior. (Opted to keep spec deltas in `lane` rather than invent a UI-framework capability.)

## Impact

- `src/qml/` — all files rewritten; `src/qml/ui/` added (~10 new files).
- `src/CMakeLists.txt`, `CMakeLists.txt` — Kirigami runtime checks dropped.
- No config, model, or C++ API changes: `Controller`/`TargetModel`/`RuleModel`/`PickerModel` contracts untouched — this keeps the C++ port (`bare-qt6-port`) independent.
- **BREAKING** (build/runtime): Kirigami and kirigami-addons are no longer runtime requirements.
- Depends on nothing in `bare-qt6-port` for its QML work, but both changes touch `src/CMakeLists.txt` and `CMakeLists.txt` — apply `bare-qt6-port` first, then rebase.
