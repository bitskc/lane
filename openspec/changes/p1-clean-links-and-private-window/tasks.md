# Implementation Tasks: Phase 1 — Clean Links & Private Window Shortcut

## 1. Clean Links (Tracking Parameter Stripper)

- [x] 1.1 Configuration plumbing:
  - Add `bool stripTrackingParams = true;` to `struct Config` in `src/core/types.h`.
  - Add `"stripTrackingParams"` to `knownKeys` set and handle JSON load/save in `src/core/config.cpp`.
  - Update `docs/config.schema.json` and sample JSON in `AGENTS.md`.
  - Verify: `QT_QPA_PLATFORM=offscreen ctest -R test_config` passes.
- [x] 1.2 Implement raw-segment query cleaner in `src/core/pipeline.cpp`:
  - Scheme guard: only clean `http` and `https` URLs.
  - Slice raw query between `?` and `#`; split on `&`.
  - Drop segments where percent-decoded key starts with `utm_` (case-insensitive) or matches banned key set (`fbclid`, `gclid`, `gbraid`, `wbraid`, `msclkid`, `twclid`, `mc_eid`, `mc_cid`, `mkt_tok`, `_ga`, `_gl`, `dclid`, `yclid`, `ttclid`, `li_fat_id`, `_hsenc`, `_hsmi`, `oly_enc_id`, `oly_anon_id`, `vero_id`, `rb_clickid`, `s_cid`, `wickedid`).
  - Drop `si` only when host is `youtube.com`, `youtu.be`, `music.youtube.com`, or `open.spotify.com`.
  - Splice surviving segments byte-for-byte; omit `?` if no query parameters survive.
  - Verify: `QT_QPA_PLATFORM=offscreen ctest -R test_pipeline` passes.
- [x] 1.3 Add test cases in `tests/test_pipeline.cpp`:
  - Standard trackers stripped (`utm_*`, `fbclid`, `gclid`).
  - Base64 padding (`?token=YWJj==&utm_source=x` -> `?token=YWJj==`).
  - Semicolon-separated values preserved without re-encoding.
  - IRI path and IDN host unencoded (`/café`, `https://bücher.de/`).
  - Case-insensitive keys (`UTM_Source=x`, `FBCLID=y`).
  - `utm_` prefix keys (`utm_id`, `utm_content`).
  - Host-scoped `si` on YouTube stripped; `si` on `example.com` preserved.
  - Non-HTTP schemes (`mailto:test@example.com?subject=hi&utm_source=x`) left unmodified.
  - O365 wrapped URLs: trackers stripped from `matchUrl`, `openUrl` preserved unless `openUnwrapped`.
  - Fragment containing `?utm_source` untouched.
  - Verify: All tests pass under `QT_QPA_PLATFORM=offscreen ctest -R test_pipeline`.
- [x] 1.4 UI & CLI plumbing:
  - Add `Q_PROPERTY(bool stripTrackingParams READ stripTrackingParams WRITE setStripTrackingParams NOTIFY stripTrackingParamsChanged)` in `src/app/Controller.h` / `Controller.cpp`.
  - In `src/app/main.cpp` (`--explain`), print removed tracking keys if URL was cleaned.
  - Add `FormSwitchDelegate` for "Clean links before routing" in `src/qml/pages/PreferencesPage.qml`.
  - Verify: Manual run with `--explain` displays stripped keys; Preferences switch toggles and persists.

## 2. Private Window Shortcut (`Alt+P`)

- [x] 2.1a Pure counterpart resolver in `src/core/router.h` / `router.cpp`:
  - Implement `const Target *privateCounterpart(const Target &target, const QList<Target> &allTargets)`.
  - Rules: already incognito -> return target; container (`:container:N`) -> strip suffix, find base `:private`; Gecko -> exact `id + ":private"`; Chromium -> exact `id + ":incognito"`; ignore candidate `hidden`; all others -> `nullptr`.
  - Add unit tests in `tests/test_router.cpp` verifying Gecko pairing, Chromium pairing, container mapping, multi-profile isolation (Work vs Personal), Brave Tor exclusion, and fail-closed null on PWAs/custom targets.
  - Verify: `QT_QPA_PLATFORM=offscreen ctest -R test_router` passes.
- [x] 2.1b Controller wiring in `src/app/Controller.h` / `Controller.cpp`:
  - Implement `Q_INVOKABLE void pickPrivate(int row)`.
  - Guard `row >= 0 && row < m_pickerModel->count()`.
  - Call `privateCounterpart(t, m_targets)`. If null, set `m_pickerNotice` and do not launch.
  - Request activation token from `m_pickerWindow`, call `requestActivationAndLaunch`, then `hidePicker()`.
  - Suppress `m_config.remembered` writes entirely.
  - In `toast()` and `launch-failed` notifications, omit `click.host` for reason `"picker-private"` to prevent notification history leaks.
- [x] 2.2 Layer-shell Picker UI in `src/qml/Picker.qml`:
  - Add `Shortcut { sequence: "Alt+P"; onActivated: if (list.count > 0 && list.currentIndex >= 0) controller.pickPrivate(list.currentIndex) }`.
  - Add `alt+p private` footer keycap label in matching lowercase monospace style with `Accessible.name: "Open in private window"`.
  - Display inline notice banner when `controller.pickerNotice` is non-empty.
  - Verify: Manual test on KWin verifying Alt+P dispatches private window and closes overlay.
- [x] 2.3 Documentation & Changelog:
  - Add `[Unreleased]` Added and Changed entries in `CHANGELOG.md` in human voice, no em dashes.
  - Note default `true` behavior change for clean links in `CHANGELOG.md`.
