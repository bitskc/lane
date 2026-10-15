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
- [ ] 2.1 Build a throwaway signed stub bundle on the MacBook and test `SMAppService.register`, `UNUserNotificationCenter` authorization, and `NSWorkspace` default-handler query with (a) ad-hoc and (b) the Apple Development identity. Record actual status strings. Cleanup is part of the task: `SMAppService.unregister`, remove the stub bundle, and note any notification permission entry it created. Dep: none.
- [ ] 2.2 Pick the signing mode for the bundle script from 2.1 and record it in `design.md` Decision 11. Dep: 2.1.

## 3. macOS build target and bundle
- [ ] 3.1 CMake: `LANE_MAC_DESKTOP` under `APPLE`; `MACOSX_BUNDLE`, OBJCXX only for `platform_macos.mm`, frameworks AppKit, UserNotifications, ServiceManagement, CoreServices; `Qt6::DBus`, `DBusActivation.cpp`, `Autostart.cpp` (reached only through `platform_linux.cpp` after 1.2) and LayerShellQt Linux-only; `DefaultBrowserWatcher.cpp` stays compiled but is never constructed on macOS; no ECM or KDE. `lane-core` gets no `.mm` files. Linux gate stays `UNIX AND NOT APPLE`. Dep: 1.3.
- [ ] 3.2 `Info.plist` template, `.icns`, version from `project(VERSION)`, `CFBundleURLTypes` http and https only, `LSUIElement`, `LSMinimumSystemVersion` 13.0. Dep: 3.1.
- [ ] 3.3 Bundle script: install, `macdeployqt -qmldir`, nested-then-bundle signing per 2.2, per-item `codesign --verify --strict`, `otool -L` check, quarantine strip, install to `~/Applications`. Dep: 3.1, 2.2.
- [ ] 3.4 `platform_macos.mm` with honest "unsupported" bodies so the app links and the picker can open; replaced by real bodies in 5 to 7. Confirm `CrashLog` runs on macOS (POSIX signals, `AppDataLocation` under `~/Library/Application Support`). Dep: 3.1.

## 4. Single instance and URL delivery
- [ ] 4.1 `SingleInstance` macOS socket path: per-user temp dir via `confstr(_CS_DARWIN_USER_TEMP_DIR)`, `lane-<uid>.sock`, 103-byte check, fail fast with message, no shared fallback. Dep: 3.1.
- [ ] 4.2 `UrlEventFilter`: install right after `QApplication`, bounded FIFO (8), http/https only, drain after `Controller` exists. Dep: 4.1.
- [ ] 4.3 Cold-start and running-instance smoke through LaunchServices with `open -b app.lane.Lane <url>` on the installed bundle; record whether the FIFO was ever used. Dep: 4.2, 3.3.

## 5. Default handler and takeover
- [ ] 5.1 `isDefaultHandler` via `URLForApplicationToOpenURL` for both schemes: bundle id plus resolved real path equal to the running bundle; states Registering, Registered, Declined, Partial, OtherCopy. `makeDefaultHandler` via `setDefaultApplicationAtURL`, then re-query. Confirm in the SDK header that the older `LSSetDefaultHandlerForURLScheme` is marked deprecated. Dep: 3.4, 2.2.
- [ ] 5.2 60-second poll only when `watchdogEnabled`; takeover alert only if Lane was previously the handler. Dep: 5.1.
- [ ] 5.3 Overview states and current-handler name (QML reads `Controller` properties). Dep: 5.1.

## 6. Overlay, activation, keys, quit and appearance
- [ ] 6.1 Overlay in the order of Decision 8: Qt window flags first, `[NSApp activate]` (or `activateIgnoringOtherApps:` on 13) after show, then collection behavior via `winId()`; re-apply on show; `closeOnFocusLoss` via resign key. `requestActivationToken` returns empty once and no XDG variable is set around a launch. Dep: 3.4.
- [ ] 6.2 Device proof on the MacBook: picker over a normal app with no click needed for keys, over a full-screen app (D5), Retina hit areas; second display only if attached (otherwise record not exercised). Screenshots in `/tmp`, not tracked. Dep: 6.1, 3.3.
- [ ] 6.3 Per-platform hint table feeding shortcuts, footer hints, empty-state hints and Accessible names (`Picker.qml:495-496`, `:531-532`, `:573-574`); system UI font for hints on macOS; type each key (⌘C, ⌥A/P/E/S/R, digits, Esc ladder, Hold Space/Return/Esc) in the real app including with the filter field focused; apply the ⌘ fallback table to any ⌥ key that fails. Dep: 6.1.
- [ ] 6.4 Light/dark live update and reduced-motion gating for the Hold ring and dim fade. Dep: 3.4.
- [ ] 6.5 Open the picker on the display containing the pointer (wins over the frontmost app's display). Dep: 6.1.
- [ ] 6.6 `Controller::requestQuit()` (cancel pending pick, else quit); status-item Quit Lane with a template (monochrome) icon that follows light and dark; `Shortcut { sequence: StandardKey.Quit }` in the Settings, picker and Hold windows; Settings opens on first launch with no `config.json` and no link, and after the pick or launch completes when the first launch was caused by a link; closing Settings hides it and the status item reopens it; `LSUIElement` verified (no Dock icon). Dep: 3.4.

## 7. Notifications and login item
- [ ] 7.1 `notify` via `UNUserNotificationCenter`: lazy authorization, delegate, bounded action map, denied banner in Overview. Dep: 3.4, 2.2.
- [ ] 7.2 `autostartSet/Enabled` via `SMAppService.mainAppService`; real status; approval and signature messages. Dep: 3.4, 2.2, 1.2.

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
