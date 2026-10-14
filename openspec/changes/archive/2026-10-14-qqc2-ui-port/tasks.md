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
- [x] `image://laneicon/<name>` uses the QML physical-pixel `sourceSize` exactly once, with theme fallback to `application-x-executable`/`image-missing`.
- [x] Register in `Controller::ensureEngine` for all three engines.

## 3. Port settings pages + shell
- [x] `pages/OverviewPage.qml`, `PreferencesPage.qml`, `RulesPage.qml`, `TargetsPage.qml` → `ScrollableFormPage` + `ui` delegates; `DragHandle` on TargetsPage.
- [x] `Settings.qml`: keep existing QQC2 ApplicationWindow/RowLayout/StackLayout shell — replace only Kirigami colors/icons. No layout rewrite.
- [x] Actual Controller + production QML rendered all pages under offscreen Qt and private KWin on 2026-10-13. Slow drag persisted order; preference glyph clicks persisted and preserved bindings; rule edits, custom-app creation and rename persisted; profile deletion removed normal/private discovery targets and visible rows. Expanded private rows showed no grip. Registered QuickTest cases cover animated drag, cancellation, no-op clicks, switch events/bindings and nested focus scrolling.

## 4. Port overlays
- [x] `Picker.qml` → QQC2/`ui` + palette colors + ThemedShadow. FULL keyboard parity: digits 1-8 in TextField.Keys.onPressed, `,`/`.`, Esc-clear-then-cancel, Alt+A/P/E, Ctrl+C, Alt+S/Alt+R zero-target strings per spec, maxRows 8.
- [x] `Hold.qml` → QQC2/`ui` + `controller.holdProgress` ProgressBar.
- [x] Manual on Wayland: layer-shell scope + exclusive keyboard work; Esc cancels; Enter confirms. CONFIRMED 2026-10-13 against `/tmp/lane-quality-private-wayland.log` (private nested KWin, not the user's compositor): `zwlr_layer_shell_v1.get_layer_surface(..., layer=3, "lane-picker")` and `"lane-hold"` each with `set_keyboard_interactivity(1)`, followed by real `wl_keyboard...enter` events on the surface (9 enters logged); trailing log line confirms the exercised sequence: "OVERLAYS Escape-clear-cancel Enter-launch Hold-Escape-to-picker".

## 5. Style + build cleanup
- [x] Delete `QQuickStyle::setStyle` in `main.cpp`; add `:/qtquickcontrols2.conf` (Style=Fusion) to qrc.
- [x] Remove `ecm_find_qmlmodule(org.kde.kirigami…)` + `…formcard` checks; verify `grep -r "org.kde" src/qml` → 0 AND `grep -c kirigami /proc/$(pidof lane)/maps` → 0 at runtime (style modules can load KF6 without imports).
- [x] Linux build and 16/16 ctest passed, including the registered production-composition QuickTest runner. Actual-app offscreen and private-KWin probes exercised the control paths and teardown with zero QML errors.
- [x] Deps/docs: PKGBUILD, CONTRIBUTING, CLAUDE.md, README, CI pacman list — drop kirigami/kirigami-addons (+ KF6 leftovers per bare-qt6-port).

## 6. Visual verification
- [x] Screenshot pass vs `docs/screenshots/*.png` on Plasma (picker, hold, settings) — flag visual deltas to user. CONFIRMED 2026-10-13: `/tmp/lane-quality-page{0..3}-final.png`, `/tmp/lane-quality-picker-final.png`, `/tmp/lane-quality-hold-final.png` inspected against `docs/screenshots/*.png`. Deltas found and flagged, both expected: style is Fusion instead of the old Breeze/Kirigami look (intentional — the point of this change), and the test run used a light theme against dark reference screenshots. Layout shows no clipping beyond normal scrolling.
- [x] `openspec validate qqc2-ui-port --strict` clean.
