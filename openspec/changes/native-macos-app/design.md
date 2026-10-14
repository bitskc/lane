# Design: native-macos-app

## Evidence base
Facts below are tagged **[probed]** (run on the MacBook or read from source this session), **[doc]** (vendor documentation or SDK header read), or **[inference]** (reasoned, must be confirmed by a task before it is relied on).

MacBook facts, 2026-10-14, `ssh andys-macbook-pro`:
- Darwin arm64, macOS 26.6.1 (25G76), Xcode with `MacOSX27.0.sdk`, Homebrew Qt 6.11.2, CMake 4.4.4, Ninja 1.13.2, `macdeployqt` present. [probed]
- Repo clone at `752f9f7`. No `lane` binary, no `Lane.app`. Core build passes 13/13 tests. [probed]
- Code-signing identities: one "Apple Development" identity and an Apple Configurator one. No Developer ID Application identity. [probed]
- Installed browsers and bundle ids [probed]:

| App | Bundle id | Executable | URL types | Notes |
|-----|-----------|------------|-----------|-------|
| Brave Browser | `com.brave.Browser` | `Brave Browser` | http, https | profiles under `~/Library/Application Support/BraveSoftware/Brave-Browser` |
| Brave Browser Beta | `com.brave.Browser.beta` | `Brave Browser Beta` | http, https | `.../Brave-Browser-Beta` |
| Microsoft Edge | `com.microsoft.edgemac` | `Microsoft Edge` | http, https | `Local State` lists `Default` and `Profile 1` |
| Zen | `app.zen-browser.zen` | `zen` | http, https | Gecko `profiles.ini`, relative `Profiles/<dir>`; Developer ID signed |
| Floorp | `org.mozilla.floorp` | `floorp` | http, https | Gecko `profiles.ini`, same layout |
| Safari | `com.apple.Safari` | `Safari` | http, https | listed at `/Applications/Safari.app` and under `/System/Cryptexes/App/System/Applications`; same bundle id |
| Perplexity | `ai.perplexity.macv3` | `Perplexity` | none | must not be offered as a browser |

- `~/Library/Application Support/Zen` and `.../zen` resolve to the same directory (case-insensitive volume). The scanner dedupes by resolved real path. [probed]
- `firefoxpwa 2.14.1` is installed; no firefoxpwa data directory exists yet, so PWA behavior on macOS is verified by fixture only. [probed]
- The current http and https handler is a third-party router, `app.loshadki.openin-setapp`. Making Lane the default displaces it, and macOS asks for consent. [probed]
- `TMPDIR` is 49 bytes, so `$TMPDIR/lane-<uid>.sock` stays far under macOS `sun_path` (104). [probed; limit is [doc]]
- One online display: built-in Liquid Retina XDR, 3024 x 1964 Retina (Apple M1 Pro). No second display was attached at probe time; no spare user account is assumed. [probed with `system_profiler SPDisplaysDataType`]

Source facts (HEAD `752f9f7`) [probed]:
- `main.cpp:138-141` claims `SingleInstance` before `Controller`; `main.cpp:164-169` makes `Lane::registerActivation` (D-Bus) fatal on failure; `src/CMakeLists.txt:119` links `Qt6::DBus`.
- `Controller.cpp:355` and `:922` call the free functions `setAutostart` and `autostartEnabled`; nothing calls `platform()->autostartSet/Enabled`, so those `Platform` methods are dead today.
- `Controller.cpp:101-104` constructs `DefaultBrowserWatcher` unconditionally.
- `SingleInstance.cpp:83` forwards `QCoreApplication::arguments()` only; `:45` reads `XDG_RUNTIME_DIR`.
- `Controller.cpp` uses `xdg-mime` at ~254 and ~707, `LayerShellQt::Window` in `configureLayerShell` (~1133), `QSystemTrayIcon` (~55).
- `discovery.cpp:1173` sets `action:email` to `xdg-email`. `core/discovery.cpp` and `core/launcher.cpp` are in `lane-core` (`src/CMakeLists.txt:1-23`), which every core test links.
- `Picker.qml` hardcodes `Alt+` shortcuts, footer hint strings, and Accessible names ("shortcut Control C", "shortcut Alt E", "shortcut Alt P").
- `Config` uses `QStandardPaths::ConfigLocation` + `/lane/config.json`.

API facts [doc, SDK headers read on the MacBook]:
- `NSWorkspace -setDefaultApplicationAtURL:toOpenURLsWithScheme:completionHandler:` is macOS 12+, asynchronous, and may show a user-consent prompt.
- `SMAppService.mainAppService` is macOS 13+; statuses Enabled, NotRegistered, RequiresApproval, NotFound; the app must be code signed; `kSMErrorInvalidSignature`, `kSMErrorLaunchDeniedByUser`, `kSMErrorAlreadyRegistered` exist.
- Qt 6.11 supports macOS 13+; `QFileOpenEvent` carries URL open requests for schemes declared in Info.plist; `QSystemTrayIcon` is supported on macOS. Qt maps ⌘ to `Qt::ControlModifier` by default (`AA_MacDontSwapCtrlAndMeta` off), so QML `Ctrl+C` is ⌘C with no code change.

Not yet verified. Each has a task that must close it before dependent work is called done: signatures sufficient for `SMAppService.register` and `UNUserNotificationCenter` on this machine; `Option+letter` delivery in a focused `TextField`; app activation and key focus for an accessory-app overlay; overlay over a full-screen Space; Edge's private flag; `ext+container` reaching Zen on macOS; `StandardKey.Quit` shortcut delivery.

## Decision 1: Qt 6 + the existing QML; native code only at the seam
Same C++ core, same `app.lane.ui` QML module, same Fusion-style controls. Objective-C++ lives in exactly one file family: `src/app/platform/platform_macos.mm`. `lane-core` stays pure Qt (no Foundation or AppKit), so every core test builds the same way on both OSes and Linux CI can run the macOS scanner tests against fixtures. Rejected: SwiftUI or AppKit rewrite (two UIs to keep in sync, loses the production-composition QML tests), Electron or Tauri (new runtime, larger bundle, no benefit for a tray daemon plus one overlay).

## Decision 2: Extend `Platform`; route every OS call through it; move Linux bodies unchanged
`platform.h` gains:

```cpp
virtual bool isDefaultHandler() = 0;                                        // http AND https
virtual void makeDefaultHandler(std::function<void(DefaultHandlerResult)> done) = 0;
virtual bool watchesDefaultHandlerByFile() const = 0;                       // Linux true, macOS false
virtual void configureOverlayWindow(QWindow *w, const QString &scope) = 0;
virtual bool registerActivation(std::function<void(const QStringList &)> handleArgs,
                                std::function<void(const QString &)> openUrl) = 0;
```

Linux wiring, all behavior-preserving:
- `isDefaultHandler`/`makeDefaultHandler`: the existing `xdg-mime` calls, moved verbatim; `done` is invoked synchronously after the existing re-query.
- `configureOverlayWindow`: the existing `configureLayerShell` body.
- `registerActivation`: wraps the existing `Lane::registerActivation` (D-Bus). `main.cpp` calls `platform()->registerActivation(...)`; its failure stays fatal on Linux. On macOS it returns true with no bus. `Qt6::DBus` and `DBusActivation.cpp` link only on Linux.
- Autostart: `Controller` stops calling the free `setAutostart/autostartEnabled` and calls `platform()->autostartSet/Enabled`. `platform_linux.cpp` already forwards to the free functions, so Linux behavior is identical. Without this step the macOS login item would never be called and `autostartEnabled()` would read a stray `.desktop` file macOS ignores.
- Watcher: `Controller` constructs `DefaultBrowserWatcher` only when `watchesDefaultHandlerByFile()` is true; otherwise it uses the poll in Decision 5.

`Controller` stops including `LayerShellQt` and stops spawning `xdg-mime`. One implementation per OS is compiled. Gate: the Linux ctest (16 tests) and the existing native smoke are green after this refactor and before any macOS file exists. If that gate fails, nothing macOS lands.

Rejected: `#ifdef Q_OS_MACOS` inside `Controller` (scatters OS code through the routing brain).

## Decision 3: URL delivery on macOS
LaunchServices hands a clicked link to the registered app as a `QFileOpenEvent` on the running instance, or starts the app and then delivers it. It never calls `lane <url>`, and it does not start a second instance of a running bundle. [doc; confirmed by the cold and running smoke in task 4.3]

```mermaid
stateDiagram-v2
    [*] --> Buffering: QApplication created, filter installed
    Buffering --> Draining: Controller constructed
    Buffering --> Buffering: QFileOpenEvent -> bounded FIFO
    Draining --> Live: FIFO empty
    Live --> Live: QFileOpenEvent -> Controller::openUrl
```

Rules:
- The filter is installed immediately after `QApplication` is constructed, before the single-instance claim and before `exec()`.
- Only `http` and `https` are accepted; anything else is dropped with a log line.
- The FIFO is bounded at 8; overflow drops the oldest and logs. Burst policy stays in `Controller` (cap 4, drop-oldest); the filter adds no second policy.
- `main.cpp` builds `Controller` before `exec()`, so in the normal path no event is delivered before readiness. The FIFO is defensive: if Qt delivers an event during startup work, it is held and drained in order. Task 4.3 measures whether it is ever used.
- Drain order is arrival order and goes through the same `Controller::openUrl` as argv and socket URLs, so rules, Hold, private routing and hold-decision behave identically.
- The CLI `Lane.app/Contents/MacOS/lane <url>` keeps argv handling and the existing socket forward (argv only, nonzero exit on an ambiguous acknowledgment, as today). No URL-list forward is added; there is no second-process path that carries `QFileOpenEvent`.

## Decision 4: Single instance socket on macOS
Same `QLocalServer` protocol and stale-socket recovery, new path rule: `<per-user temp dir>/lane-<uid>.sock`, where the per-user temp dir comes from `confstr(_CS_DARWIN_USER_TEMP_DIR)` (the directory macOS creates per user, mode 0700, not plantable by other users). If the full path exceeds 103 bytes, Lane exits nonzero with a message naming the path and length. There is no `/tmp` fallback: a predictable world-writable location lets any local user pre-create the directory and permanently block the daemon (security review finding 7). Rejected: `~/Library/Application Support` (survives reboot, so stale sockets become the normal case).

## Decision 5: Default handler
- Set: `NSWorkspace setDefaultApplicationAtURL:` for `http` then `https`, called only from the in-app button. Rejected: `LSSetDefaultHandlerForURLScheme` (older docs, including `docs/designs/port-assessment.md`, propose it; the modern asynchronous API is the supported path on macOS 12+. Task 5.1 confirms the deprecation marker in the SDK header before code is written).
- The call is asynchronous and macOS may show a consent dialog. Outcomes are distinct states in `Controller`: `Registering`, `Registered`, `Declined` (completion ran and the re-query is not Lane), `Partial` (one scheme only), and `OtherCopy` (see below). The UI never shows success from the completion callback alone.
- Query: `NSWorkspace URLForApplicationToOpenURL:` on probe `http://` and `https://` URLs. `Registered` requires the handler's bundle id to equal `app.lane.Lane` AND its resolved real path to equal the running bundle's real path. A different bundle claiming the same id (a dev build, a stale copy, or an impostor in `~/Applications`) reads as `OtherCopy`, shown with its path, and is not "Lane". This closes security finding 5 without code-signature checks.
- Takeover detection: macOS has no `mimeapps.list` to watch. When `watchdogEnabled`, a 60-second `QTimer` re-queries both schemes while Lane runs. No timer when the watchdog is off. The takeover alert and Restore action are the existing ones. A user whose handler was never Lane (here: OpenIn) gets no takeover alert, only the Overview banner naming the current handler.
- Restore never runs without a click on the notification action or the Overview button.

## Decision 6: Notifications
`UNUserNotificationCenter` with a delegate. Authorization is requested the first time Lane needs to notify. Actions are keyed by a unique notification identifier in a bounded map (same shape as the Linux map). States:
- Authorized: delivered with the Restore action.
- Denied: the notification is dropped with a log line and Settings Overview shows an inline "Notifications are off for Lane" banner with a button that opens the system notification settings. No fake success. `QSystemTrayIcon::showMessage` is not a fallback because it goes through the same permission [inference].
- Private-launch host suppression stays in `Controller` and is untouched.

## Decision 7: Login item
`SMAppService.mainAppService register/unregister`, reached through `Platform::autostartSet/Enabled` (Decision 2). `autostartEnabled()` reads `status`: Enabled is on; RequiresApproval is off with a message pointing to System Settings > General > Login Items; `kSMErrorInvalidSignature` is "needs a signed build". The Settings toggle reflects the real status after every attempt. Rejected: a hand-written LaunchAgent plist (bypasses the user-visible Login Items list).

## Decision 8: Overlay window
No compositor-exclusive grab exists on macOS and none is promised. Order of attack, each step proven on the MacBook before the next is added:
1. Qt window flags on the QML `Window` first (`Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint`, plus `Qt.Tool` only if device proof shows it is needed), because `QCocoaWindow` re-applies its own style mask and level from Qt flags on show and geometry changes.
2. Activate Lane when it shows the picker or Hold: an accessory (`LSUIElement`) app's window cannot take keyboard focus while another app is frontmost unless the app is activated [doc]. `configureOverlayWindow` calls `[NSApp activate]` (macOS 14+) or `activateIgnoringOtherApps:` (macOS 13) after the window is visible.
3. Through `winId()` to `NSWindow`, set only what Qt flags cannot: collection behavior `canJoinAllSpaces | fullScreenAuxiliary | ignoresCycle` and, if needed, level. Re-apply on every show.

No Accessibility permission, no event tap. `closeOnFocusLoss` keeps its meaning (resign key dismisses). `requestActivationToken` returns empty once and sets no XDG variables (test: no env var is set around a launch). [inference on step 3; task 6.2 proves it with a screenshot and a hit-area check, and D5's fallback is a normal top-most window on the active Space]

## Decision 9: Discovery and launch
Identity is the bundle id, not path or display name: `browser:macapp:<bundle-id>:<profile-key>`, with `:private`, `:incognito`, `:tor` and `:container:<n>` suffixes unchanged. Renaming or moving a `.app` does not orphan a user's rules.

Scanner (in `lane-core`, pure Qt):
- Roots: `/Applications` and `~/Applications`. Safari lives in `/Applications` on the probed machine; the system Cryptex root is not scanned, and duplicate bundle ids are deduped anyway.
- Candidate if `Info.plist` `CFBundleURLTypes` declares `http` or `https`. Perplexity declares none and is excluded. Lane's own bundle id is always excluded.
- `Info.plist` is read as XML with `QXmlStreamReader`. If the file starts with `bplist`, it is converted by `/usr/bin/plutil -convert xml1 -o - --` first. This keeps the parser testable on Linux with XML fixtures.
- Dedupe by bundle id (shorter path, then lexicographic) and by resolved real path.
- `CFBundleExecutable` must be a bare file name; the resolved canonical path of `Contents/MacOS/<name>` must be a regular executable file inside the bundle's canonical `Contents/MacOS` directory (no `/`, no `..`, no symlink out). A bundle that fails is skipped with a log line. Threat model: any process that can write `~/Applications` already runs code as this user; the check stops accidents and Info.plist-only tricks, not a determined same-user attacker.
- `Target` gains `bundleId` and `bundlePath` runtime fields, not serialized to `config.json`.
- Icons: `QFileIconProvider` on the bundle path through the existing `image://laneicon/` provider with a `bundle:<id>` key (Qt Widgets is already linked; no ObjC).

Profile roots become a per-OS table so Gecko, Chromium, container and PWA parsers are reused with macOS roots (`~/Library/Application Support/{BraveSoftware/Brave-Browser*,Microsoft Edge,Google/Chrome,zen,Floorp,Firefox,...}`). The Linux table is unchanged.

Launch (all in `launcher.cpp`, pure Qt):
- Gecko and Chromium families: exec the bundle's `Contents/MacOS/<CFBundleExecutable>` directly with the argv the Linux path builds, through the existing guards (sanitized HTTP(S) only, `resolveExecutable`, `isBlockedInterpreterChain`, detached start). Profile and private flags stay identical to Linux.
- Safari: `/usr/bin/open -b com.apple.Safari <url>`. No profile or private routes are invented.
- `action:email`: `/usr/bin/open <mailto-url>` where the mailto URL is `mailto:?body=<percent-encoded sanitized URL>`. The whole URL, including `&`, `=`, `?`, `#`, is encoded so an attacker URL cannot add `to`, `cc` or `subject` fields. `xdg-email` stays on Linux. On macOS the discovered email target's `exec` field is set to `/usr/bin/open` so `lane --list` prints a real binary; the launcher still builds its argv from the target kind.
- These `open` invocations are built inside the launcher from `Target.kind` and `Target.bundleId` and a fixed `/usr/bin/open` path. They never take their command text from `Target.exec` or config.
- `custom:` targets (user-authored): the macOS guard list adds, on macOS only, `open`, `osascript`, `swift`, `script`, `tclsh`, `wish`, `shortcuts`, `lldb`, `automator`, and rejects any command whose resolved path is a directory or ends in `.app`. The final symlink target's basename is checked, not only the first name. The Linux list is unchanged. A blocklist is defense in depth: `custom:` is user-authored config and the boundary is the user's own config file, same as Linux; a hardlinked copy under another name is a known limit, not a promise.
- Private flags per family: Gecko `--private-window`, Chromium `--incognito`, Edge `--inprivate`. [inference for Edge; the Edge framework string probe found `incognito` only, so task 8.3 confirms `--inprivate` or falls back to `--incognito`]
- Container targets rely on the `ext+container` handler in Zen or Firefox; whether it is reachable on macOS Zen is [inference]. Task 8.4 launches a real container target; if it does not work, container targets are hidden on macOS with a logged reason rather than offered and broken.

## Decision 10: UX on macOS
- Shortcuts stay as written in QML. `Ctrl+C` is ⌘C through Qt's default remap; `Alt+A/P/E/S/R` are Option. No `AA_MacDontSwapCtrlAndMeta`. ⌘A is not used for "always" because a focused `TextField` claims it for select-all.
- Footer hints, empty-state hints and the Accessible names/descriptions come from one per-platform table (macOS glyphs ⌘C, ⌥P, ⌥E, ⌥A, ⌥S, ⌥R; Linux strings unchanged). The spec requires "hints match the shortcuts that fire"; the glyph set lives here so the ⌘-variant fallback needs no spec change. On macOS hint labels use the system UI font, not `monospace`, and task 6.3 checks glyph rendering.
- Risk: Option+letter produces a printable character on macOS (⌥E is a dead key on US layouts). Qt matches `QKeySequence` on key code so the shortcut should fire, but a focused `TextField` could consume the character. Task 6.3 types each key in the real app. If any fail, the fallback is ⌘ variants for those keys and the table changes.
- Esc clears the field then cancels, digits pick, Return picks: unchanged and covered by existing QML tests. Hold's Space/Return/Esc are included in the typed-key check.
- Status item (D2, decided 2026-10-14): menu is {Settings, Rediscover browsers, Quit Lane}. This is a documented macOS exception to the canonical Linux "tray SHALL NOT offer Quit" rule (implemented as a MODIFIED requirement). Reason it is acceptable on macOS and not on Linux: LaunchServices relaunches Lane on the next link click when Lane is the registered handler, and a status item with no Quit is not discoverable on macOS. Template (monochrome) icon so it follows light and dark.
- One quit path, `Controller::requestQuit()`: if a pick is pending (picker or Hold), cancel it and stay running; otherwise quit. The status-item Quit, and a `Shortcut { sequence: StandardKey.Quit }` in the Settings, picker and Hold windows, all call it. This does not depend on Qt installing an app menu for an accessory app. Activating the status item opens its menu (Qt's `activated` signal is not relied on for opening Settings on macOS).
- `LSUIElement=1` (D3): no Dock icon. First run: when no `config.json` exists and no link is pending, Lane opens the Settings Overview so a new user has a visible way in. If the first launch was caused by a link, the link is routed first and Settings opens after that pick or launch completes, so the user never gets Settings and a picker at once. Closing the Settings window hides it; the daemon keeps routing; the status item or Rediscover reopens it. (Linux first-run behavior is unchanged.)
- Light and dark follow the system live (`Qt::ColorScheme`). Reduced-motion preference disables the Hold ring animation and dim fade. The picker opens on the display containing the pointer, and that wins over the frontmost app's display. Retina (device pixel ratio 2) produces nonzero hit areas for every row and footer button.

## Decision 11: Packaging and verification
- `cmake --install` plus a bundle script stages `Lane.app`: `Contents/MacOS/lane`, `Contents/Resources` (icon `.icns`, QML module), `Contents/Info.plist` from a template (`CFBundleIdentifier=app.lane.Lane`, `CFBundleURLTypes` http/https only, `LSUIElement=1`, `LSMinimumSystemVersion=13.0`, `NSHumanReadableCopyright`, version from `project(VERSION)`).
- `macdeployqt -qmldir` copies Qt frameworks and plugins. Check: `otool -L` over every Mach-O in the bundle shows no `/opt/homebrew` or `/usr/local` path.
- Signing under D1: sign each nested Mach-O, then the bundle, with ad-hoc or the Apple Development identity (task 2.1 decides which). Verify each nested item and the bundle with `codesign --verify --strict`; `--deep` is not relied on. Hardened runtime and notarization are out.
- Transfer is by `ssh`/build-on-the-Mac, which does not set `com.apple.quarantine`. The install script runs `xattr -d com.apple.quarantine` as a no-op-if-absent step in case a bundle arrived another way. A quarantined, un-notarized bundle would otherwise be blocked by Gatekeeper.
- Install target `~/Applications/Lane.app` (no sudo). The build runs on the MacBook over `ssh` (the existing `lane-macos-remote-build` flow); no binary is copied from Linux.
- `ctest` is necessary and not sufficient. A prior Lane bug (zero hit areas) passed tests that faked dimensions. The gate is a real-app smoke on the installed bundle.
- The gate cannot be fully unattended: the default-handler button must be pressed by the owner and the system consent prompt answered by the owner. Everything else (delivery, screenshot, hit areas, signature, `otool`, login-item round trip) is scripted. Task 9.3 re-runs only the scripted part.

## Risks
| Risk | Effect | Mitigation |
|------|--------|-----------|
| Local signature too weak for `SMAppService` or notifications | Autostart and Restore action fail on the MacBook | Task 2.1 spike first, with cleanup (unregister the stub); fall back to the Apple Development identity; report the actual status string |
| Qt re-applies window flags, or an accessory app cannot take key focus | Picker never gets keystrokes | Qt flags first, explicit activation, native tweaks second; typed-key device proof |
| `NSWindow` collection behavior not honored over full-screen apps | Picker hidden | D5 fallback: normal top-most window on the active Space, logged |
| Option shortcuts eaten by `TextField` | Footer hints lie | Typed-key smoke; ⌘ fallback table |
| Default-handler consent dialog dismissed | UI shows false success | Success only after both-scheme re-query; `Declined`, `Partial`, `OtherCopy` are states |
| Making Lane default displaces OpenIn | User loses a router they use today | Overview names the current handler before the click; nothing changes without the button |
| `.app` rename or move | Rules orphaned | Bundle-id identity |
| Poll timer wakes the CPU | Battery | Only when `watchdogEnabled`; 60 s |
| Linux regression from the seam refactor | Breaks the shipped platform | Linux gate before any macOS file lands |
| Homebrew Qt paths leak into the bundle | App fails on another Mac | `otool -L` check in the gate |
| Socket protocol accepts any same-uid caller's argv (`--settings`, `--rediscover`, a URL) | Same-user process can pop the picker or Settings | Same surface and same `UserAccessOption` limit as Linux; accepted, not widened |
| Single built-in display on the only test machine | Second-display behavior unverified | Scenarios are conditional on a second display; the gate records "not exercised" rather than pretending |
| Container targets may not work on macOS Zen | Broken targets offered | Task 8.4 launches a real one; hide with a logged reason if it fails |

## Review log
Round 1, 2026-10-14. Reviewers: one internal agent (`MacSpecAdversary`, product/consistency) and three `devin/swe-2` runs pinned with fallback chains emptied (engineering, security, UX). All findings were checked against source or the plan before acceptance; reviewer claims are not evidence on their own.

| # | Finding (source) | Verdict | Action |
|---|------------------|---------|--------|
| 1 | D-Bus `registerActivation` is fatal and `Qt6::DBus` is linked (eng P1) | Accepted, verified at `main.cpp:164-169`, `src/CMakeLists.txt:119` | `Platform::registerActivation`; DBus Linux-only; task 1.2 |
| 2 | Autostart bypasses the seam (eng P1) | Accepted, verified at `Controller.cpp:355,922` | Controller routes through `platform()`; task 1.2 |
| 3 | `QFileOpenEvent` cannot reach a duplicate process; Forwarding branch unreachable (eng P2) | Accepted | State machine simplified; Forwarding removed; scenario replaced by CLI forward |
| 4 | `DefaultBrowserWatcher` built unconditionally (eng P2) | Accepted, verified at `Controller.cpp:101-104` | Gated on `watchesDefaultHandlerByFile()`; task 1.2 |
| 5 | ObjC++ in `lane-core` contradicts Decision 1 (eng P2) | Accepted | Plist parsed with Qt XML plus `plutil` fallback; `open` built in launcher; icons via `QFileIconProvider`; ObjC++ only in `platform_macos.mm` |
| 6 | Reduced-motion, pointer display, appearance, quit-cancel have no implementation task (eng P2) | Accepted | New tasks 6.4, 6.5, 6.6 |
| 7 | Gate contradicts itself on `open https://...` vs untouched handler (eng P2) | Accepted | Gate uses `open -b app.lane.Lane <url>`; handler stays untouched |
| 8 | `Partial` missing from design (eng, sec P3) | Accepted | Added, plus `OtherCopy` |
| 9 | `codesign --deep` deprecated (eng P3) | Accepted | Verify each nested item and bundle |
| 10 | `CFBundleExecutable` can escape the bundle; Gatekeeper bypass (sec P1) | Accepted at P2: any same-user writer to `~/Applications` already runs code | Containment check in scanner; threat model stated |
| 11 | Blocklist bypassable (sec P1) | Partly accepted | macOS-only additions, directory and `.app` rejection, final-target basename; boundary stated honestly |
| 12 | `.app` directories pass `isExecutable` (sec P2) | Accepted, verified at `launcher.cpp` `resolveExecutable` | Code check plus test |
| 13 | `mailto:` header injection (sec P2) | Accepted | Full percent-encoding; scenario and test |
| 14 | Bundle-id-only identity enables impostor (sec P2) | Accepted | Also compare running bundle real path; `OtherCopy` state |
| 15 | `requestActivationToken` has no task or test (sec P2) | Accepted | Task 6.1 and a no-env test |
| 16 | `/tmp/lane-<uid>` fallback is a pre-creation DoS (sec P2) | Accepted | Fallback removed; per-user temp dir only; fail fast if too long |
| 17 | "Both http and https" gate hides real browsers (sec P2) | Accepted | Candidate if `http` or `https` |
| 18 | Adding `open` etc. to shared list changes Linux (sec P3) | Accepted | macOS-only additions |
| 19 | Socket argv surface (sec P3) | Accepted as documented risk | Risks table |
| 20 | `ext+container` unverified on macOS (sec P3) | Accepted | Task 8.4 |
| 21 | Accessory app needs activation for key focus (UX P1) | Accepted | Decision 8 step 2 |
| 22 | Mutating a Qt-owned `NSWindow` is fragile (UX P1) | Accepted | Decision 8 ordering: Qt flags first |
| 23 | Quit not discoverable (UX P2); Quit rationale eroded, vocabulary split (adversary P1) | Escalated to the owner; D2 decided: Quit in the status-item menu | MODIFIED "System tray presence"; one `requestQuit()` path |
| 24 | Requirement contradicts its own quit scenario (UX P2) | Accepted | Rewritten around `requestQuit()` |
| 25 | First run invisible (UX P2) | Accepted | Settings opens on first run |
| 26 | Accessible names hardcode wrong modifiers (UX P2) | Accepted | Same hint table |
| 27 | Spec pre-commits to Option glyphs (UX P2) | Accepted | Spec says "match shortcuts that fire"; glyphs in design |
| 28 | Settings close semantics unspecified (UX P2) | Accepted | Closing hides; scenario added |
| 29 | Gatekeeper/quarantine (UX P2) | Accepted | `xattr` step and transfer note |
| 30 | `monospace` may not render ⌘/⌥ (UX P3) | Accepted | System UI font on macOS; checked in 6.3 |
| 31 | Second display and fresh account cannot be verified on one machine (adversary P1, UX P3, sec cut) | Accepted | Conditional scenarios; fresh-account scenario rewritten to bundle contents plus observed prompts |
| 32 | Bundle-field serialization, single-implementation, PWA, moved-bundle, mailto scenarios missing (adversary P2, sec) | Accepted | Scenarios and test rows added |
| 33 | Gate not automatable; spike has no cleanup (adversary P2) | Accepted | Risks and task 2.1 cleanup |
| 34 | Skip the Option experiment and go straight to ⌘ (UX cut) | Rejected | ⌘A collides with text-field select-all; ⌥ keeps muscle memory; on-device test decides |
| 35 | Drop Floorp and defer Edge private flag (UX cut) | Rejected | One table row each; Edge flag is a single probe |
| 36 | Drop the Cryptex Safari root (eng, sec cut) | Accepted | Not scanned; dedupe retained |
| 37 | Drop Retina scenario (sec cut) | Rejected | The MacBook has a built-in 3024 x 1964 Retina display (probed with `system_profiler`); this is the scenario that would have caught the earlier zero-hit-area bug |

Round 2, 2026-10-14: one `devin/swe-2` run (pinned, fallbacks emptied) limited to problems the fold-in introduced. All 8 findings accepted: binary-plist fixture is macOS-only because `plutil` does not exist on Linux (the XML fixtures still run in Linux CI); "Tray activation" scoped to Linux and a macOS status-item activation scenario added; template icon assigned to task 6.6; macOS email target `exec` set to `/usr/bin/open` for a truthful `--list` (verified `discovery.cpp:1173` still says `xdg-email`); coverage row for notifications banner (5.3, 7.1); first-launch-by-link precedence added; Hold gets the quit shortcut; `Autostart.cpp` excluded from the macOS source list (reached only through `platform_linux.cpp` after task 1.2). No new Linux-regression risk found. The display probe also confirms one built-in display, so second-display scenarios stay conditional.
