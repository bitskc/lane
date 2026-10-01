# Implementation Tasks: Browser Picker Supremacy

## 1. Clean Links (Tracking Parameter Stripper)

- [ ] 1.1 Add `stripTrackingParams` (default: `true`) to `Config` struct in `src/core/types.h` and update `docs/config.schema.json`. Verify: `ctest -R test_config` passes.
- [ ] 1.2 Implement `cleanTrackingParams(const QUrl &)` in `src/core/pipeline.cpp` using `QUrlQuery` and the allowlist of banned keys. Verify: Unit tests in `tests/test_pipeline.cpp` test `utm_*`, `fbclid`, `si`, and non-tracking parameter preservation.
- [ ] 1.3 Add a "Strip tracking parameters" switch in `src/qml/pages/PreferencesPage.qml` bound to `controller.stripTrackingParams`. Verify: Toggling switch updates and persists `config.json`.

## 2. Plasma Activity-Aware Routing

- [ ] 2.1 Add `find_package(KF6Activities)` conditionally to `CMakeLists.txt` and link `KF6::Activities` if found. Verify: CMake configures cleanly on systems with and without `KF6Activities`.
- [ ] 2.2 Add `activity` field to `Rule` struct and `activityDefaults` map to `Config` in `src/core/types.h`. Verify: Serialization round-trip tests in `tests/test_config.cpp`.
- [ ] 2.3 Integrate `KActivities::Consumer` into `src/app/Controller.cpp` to track `currentActivityId` in memory. Verify: Unit test in `tests/test_matcher.cpp` asserting rule matching matches active activity ID.
- [ ] 2.4 Expose activity picker in `src/qml/pages/RulesPage.qml` and activity fallback target selection in `PreferencesPage.qml`. Verify: Creating an Activity-scoped rule in settings persists to config.

## 3. Default Browser Watchdog

- [ ] 3.1 Implement `WatchdogWatcher` in `src/app/` using `QFileSystemWatcher` on `~/.config/mimeapps.list` with a 2-second debounce timer. Verify: Touching `mimeapps.list` triggers debounced status check.
- [ ] 3.2 Wire watchdog alert to `KNotification::event` with action button "Restore Lane" calling `Controller::setDefaultBrowser()`. Verify: Simulated change triggers notification event; clicking action restores Lane in `mimeapps.list`.
- [ ] 3.3 Add `watchdogEnabled` toggle to `PreferencesPage.qml`. Verify: Disabling watchdog stops file watcher and silences notifications.

## 4. Open in Private Window (`Alt+P`)

- [ ] 4.1 Implement `launchPrivate(const QString &targetId, const QString &url)` in `src/app/Controller.cpp`. For targets with discovered incognito counterparts, launch that counterpart; otherwise append `--incognito` / `--private-window`. Verify: Unit test in `tests/test_launcher.cpp`.
- [ ] 4.2 Add `Shortcut { sequence: "Alt+P" }` in `src/qml/Picker.qml` and visual hint `"Alt+P Private"` in the footer. Verify: Pressing Alt+P in picker invokes `launchPrivate` and closes overlay.

## 5. Picker Empty State & Filter Recovery

- [ ] 5.1 In `src/qml/Picker.qml`, replace blank empty-list rectangle with explicit message branches: filter miss vs zero targets discovered. Verify: Typing random string in search shows "No matching destinations — press Esc to clear"; Esc clears text.
- [ ] 5.2 Add `Alt+S` (open settings) and `Alt+R` (rediscover) shortcuts in `Picker.qml` active during zero-target states. Verify: Pressing Alt+S opens Settings window from layer-shell overlay.

## 6. Packaging & Screenshot Refresh

- [ ] 6.1 Once AUR SSH key is added by user, run `git clone ssh://aur@aur.archlinux.org/lane.git`, push updated `PKGBUILD` and generated `.SRCINFO`. Verify: `yay -S lane` installs clean on Arch/Cachy/Garuda.
- [ ] 6.2 Recapture `docs/screenshots/{picker,hold,settings}.png` on Wayland session showing current 0.3.0 UI and badges. Verify: Image diffs show Lane branding and current Kirigami styling.
