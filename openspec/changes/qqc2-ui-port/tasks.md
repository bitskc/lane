# Tasks: qqc2-ui-port

Apply after `bare-qt6-port` merges (both touch CMake). Each task leaves the app runnable.

## 1. `ui/` module skeleton
- [ ] Create `src/qml/ui/qmldir` + `Theme.qml` singleton (spacing/radius/color tokens from `ApplicationWindow.palette`).
- [ ] `FormCard.qml`, `FormHeader.qml`, `FormDelegateSeparator.qml` with the grouped-card look.
- [ ] Six delegate components: `SwitchRow`, `ComboRow`, `ButtonRow`, `TextRow`, `TextFieldRow`, `SpinBoxRow` — FormCard-compatible property names (`text`, `description`, `checked`, `model`, etc.).
- [ ] `ThemedShadow.qml` (Rectangle + offset-shadow Rectangle, no Qt 6.8 dependency) and `DragHandle.qml` (MouseArea-driven reorder with edge auto-scroll).
- [ ] Register the `ui` module in `qt_add_qml_module`.

## 2. Port settings pages
- [ ] `pages/OverviewPage.qml` → QQC2 + `ui` delegates.
- [ ] `pages/PreferencesPage.qml` → QQC2 + `ui` delegates.
- [ ] `pages/RulesPage.qml` → QQC2 + `ui` delegates.
- [ ] `pages/TargetsPage.qml` → QQC2 + `ui` delegates + `DragHandle` for reorder.
- [ ] `pages/AboutPage.qml` (from bare-qt6-port) → `ui` delegates.
- [ ] `Settings.qml` shell → QQC2 (`ApplicationWindow` + `StackView`/`SplitView` sidebar layout replacing Kirigami page routing).
- [ ] Manual check: open Settings, verify all 5 pages render, drag-reorder + rename work, sections correct.

## 3. Port overlays
- [ ] `Picker.qml` → QQC2/`ui`: 440px card, type-to-filter, 1-9, Alt+A/Alt+P, `,`/`.` destination widening, Always checkbox.
- [ ] `Hold.qml` → QQC2/`ui`: 360px card + `controller.holdProgress` progress bar.
- [ ] Manual check on Wayland: layer-shell scope + exclusive keyboard still work; Esc cancels; Enter confirms.

## 4. Style + build cleanup
- [ ] Add `qtquickcontrols2.conf` to qrc: `Style=org.kde.desktop`, `FallbackStyle=Fusion`.
- [ ] Remove `ecm_find_qmlmodule(org.kde.kirigami...)` and `org.kde.kirigamiaddons.formcard` from CMakeLists.txt.
- [ ] Verify zero `import org.kde` remains in `src/qml/` (`grep -r "org.kde" src/qml` → 0).
- [ ] `cmake --build build` + full `ctest`; `--settings` renders with no QML warnings (offscreen run greps for TypeError/ReferenceError).

## 5. Visual verification
- [ ] Screenshot pass (existing docs/screenshots workflow): picker, hold, settings on Plasma — compare against `docs/screenshots/*.png` for glaring regressions.
- [ ] macOS rebuild sanity: lane-core + tests still green on andys-macbook-pro (UI code is qrc-embedded; the lane target doesn't build there anyway — record expected state).
- [ ] `openspec validate qqc2-ui-port --strict` clean; docs updated (README deps: Kirigami no longer required).
