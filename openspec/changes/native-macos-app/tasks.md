# Tasks: native-macos-app

Ordered. Each task names its dependency. Linux stays green after every task. Nothing in this file is started yet; the spec is reviewed first and implementation begins only on the owner's go.

Build host for all macOS work: the MacBook over `ssh andys-macbook-pro` (existing `lane-macos-remote-build` flow). Install target: `~/Applications/Lane.app`. No task changes the user's default handlers; only the in-app button does, pressed by the owner.

## 0. Review gates for this spec
- [x] 0.1 Round 1 reviews (one internal agent, three `devin/swe-2` runs) adjudicated and folded; `## Review log` in `design.md` filled in. Owner decision D2 recorded.
- [x] 0.2 Round 2 re-review (one `devin/swe-2` run, fold-in leftovers only) adjudicated; 8 findings folded (binary-plist test scope, Linux-only tray activation scenario, template icon task, email exec display, coverage row, first-launch precedence, Hold quit shortcut, dead Linux source).
- [ ] 0.3 `openspec validate native-macos-app --strict` clean; scratch-copy archive dry run clean (the change contains one MODIFIED requirement); coverage table below has no empty row.

## 1. Linux zero-regression seam refactor (blocks everything)
- [x] 1.1 Linux baseline recorded: 16/16 ctest green, `lane --list` 42 targets, `--explain` routes, D-Bus introspects org.freedesktop.Application + org.kde.KDBusService, daemon active under systemd. Dep: none.
- [x] 1.2 Seam extended and Linux bodies moved (platform.h: autostartStatus, DefaultHandlerState, watchesDefaultHandlerByFile, defaultHandlerState, makeDefaultHandler, configureOverlayWindow, registerActivation; platform_linux.cpp owns xdg-mime + layer-shell + D-Bus delegation; Controller routes through platform(); DefaultBrowserWatcher gated; main.cpp uses platform()->registerActivation; platform_macos.mm stubs added). Dep: 1.1.
- [x] 1.3 Re-run: 16/16 ctest green, --list 42 targets identical, D-Bus contract identical, daemon restarts clean, Settings loads offscreen. Review pass: 1 P1 (if/elseif merge), 1 P2 (plist key), 3 P3 fixed; diff re-verified. Dep: 1.2.

## 2. Signing and permission spike (decides D1 signing path)
- [x] 2.1 Spike run on the MacBook with a throwaway `LSUIElement` Swift bundle. Ad-hoc: `SMAppService.mainApp` register ok -> `enabled`, unregister ok -> `notRegistered`; `NSWorkspace` handler query returned `app.loshadki.OpenIn-setapp` for http and https; `UNUserNotificationCenter` settings `notDetermined` (read only, no prompt). Apple Development: not exercised, `codesign` over SSH failed `errSecInternalComponent` (locked login keychain). Cleanup done: unregistered, stub removed, login items list and notification prefs show no spike entry. Dep: none.
- [x] 2.2 Signing mode for the bundle script is ad-hoc by default with an optional `LANE_SIGN_IDENTITY` override; recorded in `design.md` Decision 11. Notification authorization (task 7.1) and the Apple Development override stay unverified until run from an unlocked GUI session. Dep: 2.1.

## 3. macOS build target and bundle
- [x] 3.1 Built on the MacBook: configure clean, `lane.app` links (arm64, Qt 6.11.2), 16/16 ctest. Fixes found by the first real build: `Info.plist.in` path (`CMAKE_SOURCE_DIR`), `CrashLog.cpp` `::sigemptyset` (Darwin macro). Dep: 1.3.
- [x] 3.2 Built `Info.plist` read back on the Mac: id `app.lane.Lane`, version 0.3.0, URL types http/https, `LSUIElement` true, min 13.0, `CFBundleIconFile` Lane. `.icns` is rendered from the SVG by `bundle.sh` (`qlmanage` + `iconutil`). Dep: 3.1.
- [x] 3.3 `packaging/mac/bundle.sh` ran to `ok` on the MacBook: icon, `macdeployqt -qmldir`, dylib IDs rewritten from `/opt/homebrew` to `@rpath` (macdeployqt leaves them absolute; caught by the `otool -L` check), nested-then-bundle ad-hoc signing, per-item `codesign --verify --strict`, quarantine strip. Install-to-`~/Applications` path written, exercised in 9.1. Dep: 3.1, 2.2.
- [x] 3.4 Honest stub shipped and then replaced by real bodies (5.1, 6.1, 7). `CrashLog` confirmed running: `~/Library/Application Support/lane/Lane/` created by the app, no crash lines during smoke. Dep: 3.1.

## 4. Single instance and URL delivery
- [x] 4.1 `singleInstanceSocketPath()` (exported): `confstr` temp dir + `lane-<uid>.sock`, empty result when over 103 bytes, `claim()` exits on empty, no fallback. `XDG_RUNTIME_DIR` still overrides so tests stay sandboxed. Real macOS finding: connecting to a stale regular file reports `UnknownSocketError` (ENOTSOCK) instead of Linux's refused, so stale reclaim now also accepts "path exists and is not a socket inode". `test_singleinstance` and `test_qml` now build and pass on macOS (15/15 then 16/16). Dep: 3.1.
- [x] 4.2 `UrlEventFilter` (bounded FIFO 8, drop-newest with count, http/https only, drain on `setSink`), installed in `main.cpp` right after `QApplication` and drained after `handleArgs`. `test_urlevents` (4 cases) passes on Linux and macOS. Also fixed a pre-existing `test_launcher` race the Mac exposed (env dump file existed before it had content). Dep: 4.1.
- [x] 4.3 Smoke on the installed bundle: `open -b app.lane.Lane <url>` cold-started the app and a window appeared; a second `open -b` while running and a direct `Contents/MacOS/lane <url>` exec both delivered into the same pid (single instance). The FIFO queue line never logged in cold start, so LaunchServices delivered the cold-start URL after `setSink` (drain path unused in practice; kept as the documented backstop). qInfo/qWarning do not reach the unified log over SSH; evidence is process count plus window presence. Dep: 4.2, 3.3.

## 5. Default handler and takeover
- [x] 5.1 `defaultHandlerState()` queries both schemes via `URLForApplicationToOpenURL`, compares bundle id AND resolved path (OtherCopy for a second Lane.app), maps Registered/Partial/OtherCopy/Declined/Unknown (Declined = a finished `makeDefaultHandler` attempt that left us non-handler). `makeDefaultHandler` chains `setDefaultApplicationAtURL` https then http and re-queries before claiming success. SDK headers confirm `URLForApplicationToOpenURL` is the live query API (not deprecated); `setDefaultApplicationAtURL` is the 12+ setter. Dep: 3.4, 2.2.
- [x] 5.2 `m_defaultPoll` QTimer (60 s) replaces the file watcher when `watchesDefaultHandlerByFile()` is false; same `watchdogEnabled` gate, same `onMimeappsChanged` takeover logic (alert only when Lane was the handler). Dep: 5.1.
- [x] 5.3 `defaultHandlerName`/`defaultHandlerStatus` properties; Overview shows the current handler's name and Partial/Declined/OtherCopy wording. Dep: 5.1.

## 6. Overlay, activation, keys, quit and appearance
- [x] 6.1 `configureOverlayWindow` sets Qt flags (Window, StaysOnTop, Frameless, accepts keys) and installs `OverlayWatcher`: on every show it sets `collectionBehavior` MoveToActiveSpace and calls `[NSApp activate]` (`activateIgnoringOtherApps:` on 13), then `requestActivate`. `closeOnFocusLoss` closes via the picker's `active` loss (`onActiveChanged`). `requestActivationToken` stays an empty single call. Dep: 3.4.
- [ ] 6.2 Device proof on the MacBook: picker over a normal app with no click needed for keys, over a full-screen app (D5), Retina hit areas; second display only if attached (otherwise record not exercised). Needs the owner at the Mac (AX is not reachable over SSH). Dep: 6.1, 3.3.
- [ ] 6.3 Per-platform hint table feeding shortcuts, footer hints, empty-state hints and Accessible names (`Picker.qml:495-496`, `:531-532`, `:573-574`); system UI font for hints on macOS; type each key (⌘C, ⌥A/P/E/S/R, digits, Esc ladder, Hold Space/Return/Esc) in the real app including with the filter field focused; apply the ⌘ fallback table to any ⌥ key that fails. Needs the owner at the Mac for the typed-key part; hint table itself is pending below. Dep: 6.1.
- [x] 6.4 `reduceMotion` property reads `accessibilityDisplayShouldReduceMotion` live and gates the Hold ring and dim-fade `Behavior`s; light/dark follows the menubar automatically (mask icon + Qt palette). Dep: 3.4.
- [x] 6.5 `showPicker` sets the picker window's screen to `QGuiApplication::screenAt(QCursor::pos())` before show. Dep: 6.1.
- [ ] 6.6 `requestQuit()` cancels a pending pick or hold, else `QCoreApplication::quit()`; status-item "Quit Lane" entry; `QIcon::setIsMask` template icon from the bundled SVG so it follows light/dark; `Shortcut { sequence: StandardKey.Quit }` in Settings, picker, and Hold; first run opens Settings with no config.json and no link, and after the pick or launch when a link started Lane (m_firstRun + maybeFirstRunSettings). LSUIElement is in the plist and verified (no Dock icon in the earlier spike). NOT verified on-device: status item visibility/click and ⌘Q delivery need the owner. Dep: 3.4.
- [ ] 7.1 `notify` via `UNUserNotificationCenter`: lazy authorization on first post, a permanent delegate routes the action button through `NotificationActionMap` (bounded LRU, 32), denied state lands `notificationsDenied` and an Overview banner. `test_notifymap` (4 cases) passes on both OSes. NOT verified: a real toast or the auth prompt; needs the owner. Dep: 3.4, 2.2.
- [ ] 7.2 `autostartSet/Enabled` via `SMAppService.mainAppService` with `AutostartStatus` mapping (enabled / requires-approval / invalid-signature) feeding `autostartMessage` on the Settings toggle. NOT verified: the register/unregister round trip on the real bundle needs the owner to toggle the switch once (login items aren't SSH-testable without AX). Dep: 3.4, 2.2, 1.2.

## 8. Discovery and launch
- [ ] 8.1 Profile-root table: Linux entries unchanged, macOS entries added; Linux fixtures identical. Dep: 1.3.
- [ ] 8.2 `.app` scanner in `lane-core`: Info.plist via `QXmlStreamReader`, with `/usr/bin/plutil` conversion for binary plists (macOS only); `http` or `https` URL type; bundle-id identity; dedupe by id and real path; executable containment check; exclude Lane; `Target` bundle fields not serialized; icons via `QFileIconProvider` on `image://laneicon/bundle:<id>`. On macOS the `action:email` target's `exec` is `/usr/bin/open` so `lane --list` is truthful; the launcher still builds its argv from the target kind and never reads that field. Dep: 8.1.
- [ ] 8.3 Launcher: Gecko and Chromium by direct bundle-executable exec; Safari via `/usr/bin/open -b`; `action:email` via `/usr/bin/open` with a fully percent-encoded `mailto:?body=`; macOS-only `custom:` guard additions including directory and `.app` rejection and final-symlink-target basename; Linux list unchanged. Confirm Edge's private flag on the MacBook. Dep: 8.2.
- [ ] 8.4 Real discovery on the MacBook lists Brave, Brave Beta, Edge, Zen, Floorp, Safari once each, no Perplexity, no Lane. Launch a real Zen container target; if `ext+container` does not take effect, hide container targets on macOS with a logged reason. Dep: 8.3.

## 9. Install and final verification
- [ ] 9.1 Install `~/Applications/Lane.app` built on the MacBook. Dep: 3.3 and 4 to 8.
- [ ] 9.2 Native gate, scripted part: cold and running link delivery with `open -b app.lane.Lane`, picker screenshot with hit-area assertion, signature and `otool -L` checks, login-item round trip, handlers recorded before and after (unchanged). Dep: 9.1.
- [ ] 9.3 Native gate, owner-attended part: owner presses the default-handler button and answers the consent prompt; notification authorization outcome observed; typed-key check; ⌘Q, status-item Quit, first run, Settings close. Anything needing hardware not present is reported as not exercised. Dep: 9.2.
- [ ] 9.4 Re-run Linux full ctest and cachy smoke. Dep: 9.3.

## 10. Docs and archive
- [x] 10.0 Dated SUPERSEDED notes added to `docs/designs/port-assessment.md`, `docs/plan-windows-port.md` and `docs/designs/windows-port-plan.md`.
- [ ] 10.1 README platform section and CHANGELOG entry after the app ships. Dep: 9.4.
- [ ] 10.2 `openspec validate --all --strict`, scratch archive dry run, then archive. Dep: 10.1.

## Test plan (real tests only; no wiring or tautology tests)
Each card says what it protects, what breaks it, and what it does not claim.

| Test | Protects | Fails when | No claim |
|------|----------|-----------|----------|
| `test_urlevents` (core-level, fake event source) | Order, bound (8), http/https filter, drop log, drain only after readiness | Reordered, unbounded, non-web URL routed | OS actually delivers events (task 4.3) |
| `test_singleinstance` macOS cases | Path rule, 103-byte fail-fast with no fallback, stale recovery | Overlong path binds somewhere else | Linux paths |
| `test_discovery_mac` over `tests/fixtures/mac/<App>.app/Contents/Info.plist` (XML fixtures run on Linux CI too; the binary-plist fixture runs only where `/usr/bin/plutil` exists and is an explicit skip elsewhere, so that scenario is covered on macOS only) | URL-type filter (http or https), bundle-id identity, dedupe, Lane exclusion, no-URL-type exclusion, executable containment (`../`, outside symlink), moved-bundle rules, bundle fields absent from saved config, binary plist read on macOS | App without URL types offered, escape accepted, rename changes id, binary plist unreadable | Real installed apps (task 8.4) |
| `test_profileroots` | Linux table identical, macOS table yields the right dirs, Gecko container and firefoxpwa fixtures produce same suffixes | Linux fixture output changes | Browser launch |
| `test_launch_mac` | `custom:` refusal for `open`, `osascript`, symlink to either, directory, `.app`; Linux `open` unchanged; non-web URL refused; Gecko/Chromium argv shape; Safari `open -b` argv; mailto body encoding including the `&to=` injection URL; no XDG env set | Guard bypass, injected mailto field | Real browser start (task 9.2) |
| `test_platform_seam` (build-level) | Exactly one backend source per OS, `Controller`/`main` free of banned calls | A banned call or second backend appears | Runtime behavior |
| Existing `test_qml` extended | Production `app.lane.ui` composition, hint table strings per platform incl. Accessible names, Esc ladder, digits | Hint and shortcut drift, zero hit area | Option-key delivery in a real OS window (task 6.3) |
| Default-handler state machine test (fake `Platform`) | Success only after both-scheme re-query at the running bundle path; Declined, Partial, OtherCopy states | Success shown from callback alone | The real consent prompt (task 9.3) |
| Quit path test (fake pending state) | `requestQuit` cancels a pending pick, otherwise quits | Quits while a pick is pending | Real menu or shortcut delivery (task 9.3) |
| Notification action map test | Bounded map, eviction, denied path drops with log | Unbounded growth, fake success | Real notification center (task 9.3) |
| Native gate scripts | LaunchServices delivery, hit rectangles, signature, `otool -L`, quarantine absent | Any listed check fails | Notarization, Intel |

Existing 13 core tests (macOS) and 16 tests (Linux) stay green unchanged.

## Requirement to task coverage
| Requirement | Tasks |
|-------------|-------|
| macOS application build | 3.1 to 3.4, 1.3, 9.1 |
| Platform default-handler, overlay, activation and autostart seam | 1.2, 1.3 |
| macOS URL delivery through LaunchServices | 4.2, 4.3 |
| macOS single instance socket | 4.1 |
| macOS default handler registration and takeover detection | 5.1 to 5.3, 2.1, 9.3 |
| macOS desktop notifications | 7.1, 2.1, 9.3 |
| macOS login item | 7.2, 2.1, 9.2 |
| macOS overlay window | 6.1, 6.2 |
| macOS application discovery | 8.1, 8.2, 8.4 |
| macOS browser launch | 8.3, 8.4 |
| macOS packaging and local signing | 3.2, 3.3, 2.2 |
| macOS native verification gate | 9.1 to 9.3 |
| lane (MODIFIED): System tray presence | 6.6, 9.3 |
| lane: macOS key hints match the real shortcuts | 6.3 |
| lane: macOS Dock presence, quit and Settings window | 6.6, 3.2, 9.3 |
| lane: macOS default-handler states in Settings | 5.3, 7.1 |
| lane: macOS appearance and displays | 6.4, 6.5, 6.2 |
