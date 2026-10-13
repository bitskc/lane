# Tasks: qqc2-ui-port

Apply after `bare-qt6-port` merges (shared CMake). App must stay runnable after each task.

## 1. `ui/` module
- [x] `laneui` STATIC target + `qt_add_qml_module(laneui URI app.lane.ui)` in `src/CMakeLists.txt`, linked into `lane`; NO hand-written qmldir.
- [x] `Theme.qml` singleton: metrics + semantic colors only (no window/palette access — call sites use attached `palette`).
- [x] `ScrollableFormPage.qml` (Flickable + centered card-width column + ensureVisible on focus), `FormCard.qml`, `FormHeader.qml`, `FormDelegateSeparator.qml`.
- [x] `FormSwitchDelegate`, `FormComboBoxDelegate`, `FormButtonDelegate`, `FormTextDelegate`, `FormTextFieldDelegate`, `FormSpinBoxDelegate` — FormCard property-shape parity.
- [x] `ThemedShadow.qml` (Rectangle + `QtQuick.Effects.MultiEffect`, Qt 6.5+ OK) matching picker's shadow.size 32/yOffset 10/alpha .42.
- [x] `DragHandle.qml`: disabled while filtering, captures dragId before first move, edge auto-scroll, release → `controller.moveTarget`.

## 2. Icon provider (C++)
- [x] `src/app/ThemeIconProvider.*`: `image://laneicon/<name>?size=N` → `QIcon::fromTheme` pixmap @ size × devicePixelRatio, fallback `application-x-executable`.
- [x] Register in `Controller::ensureEngine` for all three engines.

## 3. Port settings pages + shell
- [x] `pages/OverviewPage.qml`, `PreferencesPage.qml`, `RulesPage.qml`, `TargetsPage.qml` → `ScrollableFormPage` + `ui` delegates; `DragHandle` on TargetsPage.
- [x] `Settings.qml`: keep existing QQC2 ApplicationWindow/RowLayout/StackLayout shell — replace only Kirigami colors/icons. No layout rewrite.
- [x] Manual: all pages render, scroll on small windows, drag-reorder + rename + private-section invariants hold.

## 4. Port overlays
- [x] `Picker.qml` → QQC2/`ui` + palette colors + ThemedShadow. FULL keyboard parity: digits 1-8 in TextField.Keys.onPressed, `,`/`.`, Esc-clear-then-cancel, Alt+A/P/E, Ctrl+C, Alt+S/Alt+R zero-target strings per spec, maxRows 8.
- [x] `Hold.qml` → QQC2/`ui` + `controller.holdProgress` ProgressBar.
- [x] Manual on Wayland: layer-shell scope + exclusive keyboard work; Esc cancels; Enter confirms.

## 5. Style + build cleanup
- [x] Delete `QQuickStyle::setStyle` in `main.cpp`; add `:/qtquickcontrols2.conf` (Style=Fusion) to qrc.
- [x] Remove `ecm_find_qmlmodule(org.kde.kirigami…)` + `…formcard` checks; verify `grep -r "org.kde" src/qml` → 0 AND `grep -c kirigami /proc/$(pidof lane)/maps` → 0 at runtime (style modules can load KF6 without imports).
- [x] `cmake --build build` + `ctest` regression gate; `--settings` offscreen run greps for TypeError/ReferenceError/binding-loop warnings.
- [x] Deps/docs: PKGBUILD, CONTRIBUTING, CLAUDE.md, README, CI pacman list — drop kirigami/kirigami-addons (+ KF6 leftovers per bare-qt6-port).

## 6. Visual verification
- [x] Screenshot pass vs `docs/screenshots/*.png` on Plasma (picker, hold, settings) — flag visual deltas to user.
- [x] `openspec validate qqc2-ui-port --strict` clean.
