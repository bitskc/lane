# Tasks: native-macos-app

Ordered. Each task names its dependency. Linux stays green after every task. Nothing in this file is started yet; the spec is reviewed first and implementation begins only on the owner's go.

Build host for all macOS work: the MacBook over `ssh andys-macbook-pro` (existing `lane-macos-remote-build` flow). Install target: `~/Applications/Lane.app`. No task changes the user's default handlers; only the in-app button does, pressed by the owner.

## 0. Review gates for this spec
- [x] 0.1 Round 1 reviews (one internal agent, three `devin/swe-2` runs) adjudicated and folded; `## Review log` in `design.md` filled in. Owner decision D2 recorded.
- [x] 0.2 Round 2 re-review (one `devin/swe-2` run, fold-in leftovers only) adjudicated; 8 findings folded (binary-plist test scope, Linux-only tray activation scenario, template icon task, email exec display, coverage row, first-launch precedence, Hold quit shortcut, dead Linux source).
- [x] 0.3 `openspec validate --all --strict` clean (3/3); scratch-copy archive dry run clean (+12 cross-platform, +4 and ~1 lane, post-archive validate clean); coverage table has no empty cell. Fixed on the way: tasks 7.1 and 7.2 sat under the group-6 heading (missing `## 7.` heading), which strict mode reports as a warning.

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
- [x] 5.1 `defaultHandlerState()` queries both schemes via `URLForApplicationToOpenURL`, compares bundle id AND resolved path (OtherCopy for a second Lane.app), maps Registered/Partial/OtherCopy/Declined/Unknown (Declined = a finished `makeDefaultHandler` attempt that left us non-handler). `makeDefaultHandler` calls `setDefaultApplicationAtURL:toOpenContentType:public.url` (the `toOpenURLsWithScheme:` variant returns permErr on macOS 26 — Decision 5 amended) and re-queries both schemes before claiming success. `public.html` is NOT claimed — it would make Lane the local .html default, which file:// opens cannot serve. SDK headers confirm `URLForApplicationToOpenURL` is the live query API. Dep: 3.4, 2.2.
- [x] 5.2 `m_defaultPoll` QTimer (60 s) replaces the file watcher when `watchesDefaultHandlerByFile()` is false; same `watchdogEnabled` gate, same `onMimeappsChanged` takeover logic (alert only when Lane was the handler). Dep: 5.1.
- [x] 5.3 `defaultHandlerName`/`defaultHandlerStatus` properties; Overview shows the current handler's name and Partial/Declined/OtherCopy wording. Dep: 5.1.

## 6. Overlay, activation, keys, quit and appearance
- [x] 6.1 `configureOverlayWindow` sets Qt flags (Window, StaysOnTop, Frameless, accepts keys) and installs `OverlayWatcher`: on every show it sets `collectionBehavior` MoveToActiveSpace and calls `[NSApp activate]` (`activateIgnoringOtherApps:` on 13), then `requestActivate`. `closeOnFocusLoss` closes via the picker's `active` loss (`onActiveChanged`). `requestActivationToken` stays an empty single call. Dep: 3.4.
- [ ] 6.2 Device proof on the MacBook: picker over a normal app with no click needed for keys, over a full-screen app (D5), Retina hit areas; second display only if attached (otherwise record not exercised). Needs the owner at the Mac (AX is not reachable over SSH). Dep: 6.1, 3.3.
- [x] 6.3 (code part) Per-platform hint table landed in Picker.qml: `mac` flag, `hintCopy` (⌘C/^C), `hintAltP` (⌥P/alt+p), `hintAltE` (⌥E/alt+e), system-font hints on macOS vs monospace elsewhere; Accessible names switch Control/Command and Alt/Option wording. Qt.ControlModifier is ⌘ on macOS so no key bindings changed; `StandardKey.Quit` shortcuts are macOS-gated via `quitShortcutEnabled`. Owner-needed before the requirement closes: typing each key (⌘C, ⌥A/P/E/S/R, digits, Esc ladder, Hold Space/Return/Esc) in the real app including with the filter focused; apply the ⌘ fallback table to any ⌥ key that fails. Dep: 6.1.
- [x] 6.4 `reduceMotion` property reads `accessibilityDisplayShouldReduceMotion` live and gates the Hold ring and dim-fade `Behavior`s; light/dark follows the menubar automatically (mask icon + Qt palette). Dep: 3.4.
- [x] 6.5 `showPicker` sets the picker window's screen to `QGuiApplication::screenAt(QCursor::pos())` before show. Dep: 6.1.
- [ ] 6.6 `requestQuit()` cancels a pending pick or hold, else `QCoreApplication::quit()`; status-item "Quit Lane" entry; `QIcon::setIsMask` template icon from the bundled SVG so it follows light/dark; `Shortcut { sequence: StandardKey.Quit }` in Settings, picker, and Hold; first run opens Settings with no config.json and no link, and after the pick or launch when a link started Lane (m_firstRun + maybeFirstRunSettings). LSUIElement is in the plist and verified (no Dock icon in the earlier spike). NOT verified on-device: status item visibility/click and ⌘Q delivery need the owner. Dep: 3.4.

## 7. Notifications and login item
- [ ] 7.1 `notify` via `UNUserNotificationCenter`: lazy authorization on first post, a permanent delegate routes the action button through `NotificationActionMap` (bounded LRU, 32), denied state lands `notificationsDenied` and an Overview banner. `test_notifymap` (4 cases) passes on both OSes. NOT verified: a real toast or the auth prompt; needs the owner. Dep: 3.4, 2.2.
- [ ] 7.2 `autostartSet/Enabled` via `SMAppService.mainAppService` with `AutostartStatus` mapping (enabled / requires-approval / invalid-signature) feeding `autostartMessage` on the Settings toggle. NOT verified: the register/unregister round trip on the real bundle needs the owner to toggle the switch once (login items aren't SSH-testable without AX). Dep: 3.4, 2.2, 1.2.

## 8. Discovery and launch
- [x] 8.1 Profile-root table: per-OS `rootRules` table in `discovery.cpp` (`linux` candidates under configHome/home unchanged, `mac` under applicationSupport); `DiscoveryPaths` gains `bundleDirs` + `applicationSupport`; `test_profileroots` pins both sides. Dep: 1.3.
- [x] 8.2 `.app` scanner in `lane-core` (`scanMacBundles`): XML Info.plist via `QXmlStreamReader`, `plutil` conversion macOS-only (clean skip elsewhere); http/https URL-type filter; `browser:macapp:<bundle-id>:<key>` ids; dedupe by bundle id (shorter path) and canonical path; executable containment inside `Contents/MacOS`; Lane excluded; `Target.bundleId`/`bundlePath` runtime-only (not serialized); icon key `bundle:<id>`; `action:email` exec `/usr/bin/open` on macOS; Edge bundle gets `--inprivate`. `test_discovery_mac` covers the table entries. Dep: 8.1.
- [x] 8.3 Launcher (via `MacLauncher` subagent, verified): Gecko/Chromium exec the bundle binary directly; Safari goes through `/usr/bin/open -b com.apple.Safari`; `action:email` is `/usr/bin/open` with a fully percent-encoded `mailto:?body=`; macOS-only custom-target guards (open/osascript/swift/script/tclsh/wish/shortcuts/lldb/automator, directory and .app rejection, per-hop symlink basenames) live in `blockedChain(macRules)`; Linux chain unchanged; Edge `--inprivate` confirmed on the MacBook (`--incognito` opens a normal window there). `test_launch_mac` 15 cases. Dep: 8.2.
- [x] 8.4 `lane --list` on the bundled app on the MacBook shows 47 targets: Brave (2 profiles + incognito + tor), Brave Beta, Edge (2 profiles + incognito), Zen (containers), Floorp (containers), Safari once each; Perplexity and Lane absent. Real fix landed here: Apple `chflags hidden` on `/Applications/Safari.app` (cryptex symlink) meant `QDir` never listed it without `QDir::Hidden`. Caveat recorded: iTerm2 declares http/https and legitimately appears as a Generic target; container launch on macOS still needs an owner-run pick (ext+container protocol claim). Dep: 8.3.

## 9. Install and final verification
- [x] 9.1 `/Applications/Lane.app` installed from the build on the MacBook via `bundle.sh build-mac /Applications` (ditto + in-place signing); moved from `~/Applications` because the System Settings browser list only scans `/Applications`. Dep: 3.3, 4-8.
- [x] 9.2 Scripted gate on the installed bundle: `codesign --verify --deep --strict` passes (valid on disk + designated requirement); `otool -L` shows zero `/opt/homebrew` refs; `file` reports Mach-O arm64; handlers unchanged (http+https still `app.loshadki.openin-setapp`); `open -b app.lane.Lane` cold-starts the app and a second `open -b` while running delivers into the same pid. Real fix landed here: stale LaunchServices registrations for stray Lane.app copies (build tree, /tmp, test fixture) stole `open -b`; `lsregister -u` cleared them and `tests/fixtures/mac/NOTE.md` documents the residue risk. Installed path is `/Applications/Lane.app` (the Settings browser list scans `/Applications`, not `~/Applications`; D1 install location updated accordingly). Owner-attended remainder stays in 9.3. Dep: 9.1.
- [ ] 9.3 Owner-attended gate: press the default-handler button, answer the consent prompt, observe notification authorization outcome, type each key (⌘C, ⌥A/P/E/S/R, digits, Esc ladder, Hold Space/Return/Esc), ⌘Q + status-item Quit, first-run Settings, Settings close/reopen, login-item toggle, picker over a normal app and a full-screen app, Retina hit areas. Anything needing hardware not present is reported as not exercised. Dep: 9.2.
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
