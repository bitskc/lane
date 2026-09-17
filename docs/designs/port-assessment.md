# Lane macOS / Windows port feasibility

Grounding: full read of `src/`, `CMakeLists.txt`, `data/`, `tests/`, and `docs/designs/competitive-analysis.md` on 2026-09-16. External claims are marked **[verified]** when backed by a fetched primary source in this session, **[inference]** when reasoned from code plus general platform knowledge.

Lane today is a Linux-only Plasma link router. The codebase has **zero** `#ifdef Q_OS_*`, `if(UNIX AND NOT APPLE)`, or other platform branches. Every OS-specific behavior is implicit: it assumes Freedesktop `.desktop` files, D-Bus activation, XDG default-browser tools, and Wayland layer-shell. A port is a platform-abstraction project, not a recompile.

---

## Coupling inventory

| Surface | File:line | Linux mechanism today | macOS equivalent | Windows equivalent | Seam? |
|---|---|---|---|---|---|
| Picker overlay | `src/qml/Picker.qml:6-20`, `src/app/Controller.cpp:817-837,860-883` | LayerShell QML import + `LayerShellQt::Window` full-screen overlay, exclusive keyboard | Borderless `NSPanel` (nonactivating) or `QWindow` with `Qt::WindowStaysOnTopHint`; capture keys via focus, not compositor protocol | Borderless topmost `QWindow` (`HWND_TOPMOST`, `WS_EX_TOOLWINDOW`); global hotkey fallback if focus steal fails | **Scattered** — QML imports layer-shell directly; C++ duplicates config in `configureLayerShell()` |
| Hold HUD overlay | `src/qml/Hold.qml:6-20`, `Controller.cpp:968-988,860-883` | Same layer-shell stack as picker | Same as picker | Same as picker | **Scattered** — second QML file with duplicate layer-shell properties |
| Layer-shell fallback | `Controller.cpp:862-865` | Logs warning when `LayerShellQt::Window::get()` returns null (X11/non-wlroots) | Always null; falls back to normal always-on-top window | Always null; same fallback | Partial — one function, but QML still imports layer-shell unconditionally |
| Single resident instance | `src/app/main.cpp:143-151`, `data/dbus/app.lane.Lane.service.in:1-3`, `data/systemd/lane.service.in:7-9` | `KDBusService::Unique` on bus name `app.lane.Lane`; systemd `Type=dbus` unit | `QLocalServer` / shared memory / `LSMultipleInstancesProhibited` in Info.plist plus socket IPC | Named mutex + `QLocalServer` (`\\.\pipe\lane` or `%LOCALAPPDATA%\Lane\lane.sock`) | **None** — hard-wired in `main.cpp` |
| URL delivery (first launch) | `data/app.lane.Lane.desktop.in:6`, `main.cpp:153`, `Controller.cpp:309-338` | Desktop `Exec=@LANE_BIN@ %u` passes URL as argv | `Info.plist` `CFBundleURLTypes` for `http`/`https`; `QFileOpenEvent` / `application:openURLs:` | Registry `HKCR\Lane\URL\shell\open\command`; each click spawns new process unless IPC forwards | **Partial** — `handleArgs()` parses argv generically, but daemon path assumes D-Bus not a second process |
| URL delivery (daemon running) | `main.cpp:144-150`, `Controller.cpp:340-401` | `KDBusService::activateRequested` / `openRequested`; `XDG_ACTIVATION_TOKEN` in env | Forward URL over local socket to resident instance; no activation token | Same IPC; no UserChoice-style token concept | **None** — D-Bus signals are the only second-instance path |
| Activation token / focus handoff | `Controller.cpp:342-352,768-794`, `launcher.cpp:245-267`, `launcher.h:9-12` | `KWaylandExtras::xdgActivationToken()` then `XDG_ACTIVATION_TOKEN` env for child | `[inference]` `NSRunningApplication activateWithOptions:` or let browser steal focus; no XDG token | `[inference]` `AllowSetForegroundWindow` / `ShellExecute`; no XDG token | **Scattered** — token logic in Controller + launcher with no abstraction |
| Default-browser check | `Controller.cpp:164-175`, `OverviewPage.qml:18-26` | `xdg-settings get default-web-browser` vs `app.lane.Lane.desktop` | `[verified]` `LSCopyDefaultHandlerForURLScheme` for `http`, `https` | Read `UserChoice` ProgId under `...\UrlAssociations\http` (check only) | **None** — inline `QProcess` to `xdg-settings` |
| Default-browser set | `Controller.cpp:472-482` | `xdg-mime default` + `xdg-settings set default-web-browser` | `[verified]` `LSSetDefaultHandlerForURLScheme` — can set programmatically | **Cannot set** since Win8; open `ms-settings:defaultapps` **[verified]** Microsoft docs | **None** |
| Autostart | `Autostart.cpp:15-53` | XDG `~/.config/autostart/app.lane.Lane.desktop` | `~/Library/LaunchAgents/app.lane.Lane.plist` or SMAppService (macOS 13+) | `HKCU\Software\Microsoft\Windows\CurrentVersion\Run` | **None** |
| System tray | `Controller.cpp:51-65` | `KStatusNotifierItem` (Freedesktop StatusNotifierItem D-Bus) | `QSystemTrayIcon` / `NSStatusItem` | `QSystemTrayIcon` / `Shell_NotifyIcon` | **None** |
| Notifications | `Controller.cpp:685-690,748-754,797-814` | `KNotification` + `data/app.lane.Lane.notifyrc` | Qt tray message or macOS User Notifications | Qt tray message or Win10 toast | **None** |
| Crash handler | `main.cpp:67` | `KCrash` | `[inference]` optional / drop | Same | **None** |
| Browser discovery (outer layer) | `discovery.cpp:134-200,880-887` | Scan `ApplicationsLocation` for `.desktop` files | Scan `/Applications/*.app/Contents/Info.plist` | Registry App Paths, Start Menu | **Partial** — `DiscoveryPaths` injectable; `scanDesktopFiles()` Linux-only |
| Browser profile paths | `discovery.cpp:297-384` | `~/.config/zen`, `~/.mozilla/firefox`, `~/.var/app/...` | `~/Library/Application Support/Firefox`, Zen paths | `%APPDATA%\Mozilla\Firefox`, `%LOCALAPPDATA%\...\User Data` | **Partial** — path lists in `fingerprint()` need per-platform tables |
| Flatpak handling | `discovery.cpp:249-324` | `flatpak run` exec prefix + `~/.var/app/` | N/A | N/A | Linux-only |
| Container targets | `discovery.cpp:468-577` | `ext+container:...` argv | Same if extension installed **[verified]** | Needs `ext+container` registry handler **[verified]** | Mostly free at launch layer |
| PWA / firefoxpwa | `discovery.cpp:799-851` | `GenericDataLocation/firefoxpwa/config.json` | Same path layout **[verified]** PWAsForFirefox docs | `%APPDATA%\FirefoxPWA\config.json` **[verified]** | Partial — icon lookup uses `.desktop` |
| Email action | `discovery.cpp:873` | `xdg-email` | `mailto:` / `NSWorkspace` | `mailto:` / `ShellExecute` | **None** |
| Source-app rules | `SourceInfo.cpp:6-11` | Empty stub (Wayland dead end) | `[inference]` frontmost-app APIs | `[inference]` `GetForegroundWindow` | Same stub |
| Config / updates / unshorten | `config.cpp:160`, `UpdateChecker.cpp`, `unshorten.cpp` | Qt standard | Same | Same | **Free** |
| Build / install | `CMakeLists.txt:43-86` | LayerShellQt, D-Bus, systemd, `.desktop` | Info.plist, signing, notarization | Registry handlers, Authenticode | **None** |

**Seam summary:** Only `DiscoveryPaths` + `discoverTargets()` is cleanly injectable (tests use it). Overlay, IPC, default-browser, autostart, tray, and notifications are woven through `Controller.cpp`, `main.cpp`, and QML. Expect a new `src/platform/` module.

---

## What's free

| Module | Files | Notes |
|---|---|---|
| URL safety | `urlutil.cpp` | http/https gate |
| Matcher / pipeline / router | `matcher.cpp`, `pipeline.cpp`, `router.cpp`, `destination.cpp` | Rules, hold, remembered hosts |
| Config | `config.cpp` | JSON; `QStandardPaths` for path |
| Launcher argv | `launcher.cpp:82-105` | `$url`, `$urlEncoded`, `ext+container:` |
| Profile parsing | `discovery.cpp:400-780` | OS-agnostic once paths are right |
| Update checker / unshorten | `UpdateChecker.cpp`, `unshorten.cpp` | QNetwork only |
| CLI | `main.cpp:111-140` | `--list`, `--explain`, `--config-path` |
| Tests | 10 of 12 binaries | `test_discovery` / `test_launcher` need fixture paths only |

Roughly **~60% of `src/core/`** ports without logic changes.

---

## What must be built per platform

### Shared

1. CMake conditionals: drop LayerShellQt, KDBusAddons, Qt6 DBus on non-Linux.
2. `Platform` interface: overlay, single-instance, default-browser, autostart, tray, notifications.
3. QML without `org.kde.layershell` (or per-OS QML trees).
4. IPC for URL forwarding to resident daemon.
5. Discovery path tables per OS (reuse parsers).
6. Replace `xdg-email` action target.

### macOS

- Overlay: NSPanel / topmost QWindow; keyboard routing without layer-shell exclusive grab.
- Single instance: `QLocalServer` + `QFileOpenEvent`.
- Default browser: `LSSetDefaultHandlerForURLScheme` (one-click still possible).
- Discovery: `/Applications`, `~/Library/Application Support/...`.
- Autostart: LaunchAgent plist.
- Packaging: signed app, notarized dmg.

### Windows

- Overlay: topmost borderless window; multi-monitor QA.
- Single instance: mutex + named pipe; **every click may spawn a process** until IPC exists.
- Default browser: installer registers handlers; UI opens Settings only (no programmatic set).
- Discovery: registry App Paths, `%LOCALAPPDATA%`, Start Menu.
- Autostart: Run key.
- Packaging: Authenticode, SmartScreen wait.

---

## KF6 dependency matrix

| Dependency | macOS/Windows | Action |
|---|---|---|
| Kirigami, I18n, CoreAddons, IconThemes, ColorScheme, qqc2-desktop-style | Available via Craft **[verified]** | Keep |
| KNotifications | Builds; behavior varies | Replace or accept |
| KStatusNotifierItem | Linux D-Bus only **[verified]** | Use `QSystemTrayIcon` |
| KDBusService / DBusAddons | Linux only | Replace with IPC |
| KWaylandExtras | Wayland only | Stub |
| LayerShellQt | Wayland wl-layer-shell only **[verified]** | Remove; platform overlay |

NeoChat and other KF6 Kirigami apps ship on Windows/macOS via Craft, but none depend on layer-shell for core UX. Lane is more Plasma-coupled than they are.

---

## External research

**firefoxpwa:** Windows MSI, macOS Homebrew, shared CLI and config.json **[verified]** (PWAsForFirefox README).

**ext+container:** Cross-platform Firefox extension **[verified]** (AMO, `honsiorovskyi/open-url-in-container`). Lane's argv format already matches. Windows needs one-time protocol registration.

**Competitors:** Velja/Choosy (mac), Browser Tamer (Windows), Junction (Linux/GNOME) per `competitive-analysis.md`. Lane's hold HUD and path-scoped memory are unique but not platform-locked in code.

---

## Effort estimate

### macOS: **8-12 person-weeks** (+2-4 tail)

| Component | Weeks |
|---|---|
| Platform seam + Craft | 1.5-2 |
| IPC + URL events | 1.5-2 |
| Overlay picker + hold | 2-3 |
| Discovery paths | 1-1.5 |
| Default browser + autostart + tray | 0.5-1 |
| Notifications swap | 0.5 |
| Signing / notarization | 1-2 |
| QA | 1-2 |

**Hardest component:** overlay without layer-shell (`Picker.qml:17-18`, `Controller.cpp:874`).

### Windows: **10-14 person-weeks** (+3-5 tail)

| Component | Weeks |
|---|---|
| Platform seam + MSVC/Craft | 2 |
| Mutex + pipe IPC | 2-3 |
| Overlay | 2-3 |
| Discovery | 1.5-2 |
| Default-browser UX (no one-click set) | 1 |
| Installer + signing | 1.5-2 |
| QA (incl. ARM64) | 1.5-2 |

**Hardest component:** second-process URL IPC + default-browser onboarding (`main.cpp:143-153`, no Win analogue to `makeDefaultBrowser()`).

Ongoing: cross-platform CI and ~0.25 FTE maintenance.

---

## Strategic note

Porting means competing with Velja, Choosy, and Browser Tamer in mature markets while giving up Lane's Plasma wedge (layer-shell, future KActivities routing). The competitive-analysis doc already recommends Plasma-only features competitors cannot copy without becoming KDE apps. **[inference]** Port only makes sense for deliberate cross-platform expansion, not for winning link routing on Linux.

---

## Recommendation

**Do not port** now. If forced later: **macOS first** (programmatic default handler, simpler IPC story), then Windows only after macOS proves demand.

Invest in Plasma-only depth instead of parity fights on Mac/Windows.

---

## Report summary

| Platform | Effort | Hardest component |
|---|---|---|
| macOS | 8-12 pw (+2-4 tail) | Layer-shell-free overlay with keyboard-exclusive picker |
| Windows | 10-14 pw (+3-5 tail) | URL IPC + non-programmatic default-browser flow |

**Recommendation:** Do not port.
