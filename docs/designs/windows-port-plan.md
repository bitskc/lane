# Lane Windows port plan

Grounding: `docs/designs/port-assessment.md` (coupling inventory), `docs/designs/competitive-analysis.md` (differentiators to preserve), and direct reads of `src/app/Controller.cpp`, `src/app/main.cpp`, `src/core/launcher.cpp`, `src/core/discovery.cpp`, `src/qml/Picker.qml`, `src/qml/Hold.qml`, `CMakeLists.txt` on 2026-09-16. No Mac port is planned; this document is Windows-only.

Lane's Windows competitor is Browser Tamer (`bt`). Lane wins on path-scoped memory, hold HUD, native container discovery, and agent-friendly config. Those features are not platform-locked in code, but the Linux integration layer is. The port is a platform-abstraction project, not a recompile.

---

## 1. Architecture: `src/platform/`

Introduce a small `Platform` interface with exactly the surfaces the port assessment found. No speculative hooks (no scripting bridge, no source-app APIs, no update-checker abstraction).

### Interface (proposed)

```cpp
// src/platform/Platform.h
class Platform {
public:
    virtual ~Platform() = default;

    // Overlay: configure a QWindow after QML load (picker or hold).
    virtual void configureOverlay(QWindow *window, const QString &scope) = 0;

    // Single instance: start listening; returns false if another instance owns the lock.
    // Second-instance argv/URL delivery calls the provided handler on the resident instance.
    virtual bool ensureSingleInstance(std::function<void(const QStringList &)> onActivate) = 0;
    virtual void sendToRunningInstance(const QStringList &args) = 0;

    // Default browser: read-only check + UX for "make default".
    virtual bool isDefaultBrowser() const = 0;
    virtual void openDefaultBrowserSettings() = 0;  // Linux: xdg-* set; Windows: ms-settings:defaultapps

    // Autostart
    virtual bool autostartEnabled() const = 0;
    virtual void setAutostart(bool on) = 0;

    // Tray (returns QObject* owned by platform impl, or nullptr if tray unavailable)
    virtual QObject *createTrayIcon(QObject *parent) = 0;

    // Notifications (thin wrapper so Controller stops including KNotification directly)
    virtual void notify(const QString &eventId, const QString &title, const QString &text,
                        const QString &iconName) = 0;

    // Focus handoff when launching a browser after picker/hold
    virtual void requestActivationToken(QWindow *window, std::function<void(const QString &)> finish) = 0;
    virtual void applyActivationForChildProcess(const QString &token) = 0;  // env var on Linux; ASFW on Windows

    // Discovery roots (feeds existing parsers in discovery.cpp)
    virtual DiscoveryPaths discoveryPaths() const = 0;

    // Action targets that differ by OS (email handler, etc.)
    virtual QList<Target> platformActionTargets() const = 0;
};

Platform *createPlatform(QObject *parent = nullptr);  // factory in platform/Platform.cpp
```

`DiscoveryPaths` already exists in `src/core/discovery.h:8-13`. Keep it; only the provider moves behind `Platform::discoveryPaths()`.

### Call sites that move behind `Platform`

| Surface | Current location | Moves to |
|---|---|---|
| Picker overlay config | `Controller.cpp:817-837` (`ensurePickerEngine`), `860-883` (`configureLayerShell`) | `Platform::configureOverlay` |
| Hold overlay config | `Controller.cpp:968-988` (`ensureHoldEngine`), same `configureLayerShell` | `Platform::configureOverlay` |
| Layer-shell QML properties | `Picker.qml:6-20`, `Hold.qml:6-20` | Per-platform QML (see section 3) |
| Single instance + URL IPC | `main.cpp:143-151` (`KDBusService`) | `Platform::ensureSingleInstance` / `sendToRunningInstance` |
| argv URL on first launch | `main.cpp:153`, `Controller.cpp:309-338` (`handleArgs`) | Stays in Controller; IPC path calls same `handleArgs` |
| Second-instance URL forward | `main.cpp:144-150`, `Controller.cpp:340-401` (`openUrl` queue) | IPC handler invokes `handleArgs` / `openUrl`; re-entrancy guard stays |
| Activation token capture | `Controller.cpp:342-352` | Stays in Controller (generic); token source is platform |
| Activation token request | `Controller.cpp:768-794` (`requestActivationAndLaunch`) | `Platform::requestActivationToken` |
| Child-process focus env | `launcher.cpp:245-267` (`XDG_ACTIVATION_TOKEN`) | `Platform::applyActivationForChildProcess` |
| Default-browser check | `Controller.cpp:164-175`, `OverviewPage.qml:18-26` | `Platform::isDefaultBrowser` |
| Default-browser set UX | `Controller.cpp:472-482`, `OverviewPage.qml:24-27` | `Platform::openDefaultBrowserSettings` (rename `makeDefaultBrowser` in Controller to match intent) |
| Autostart | `Autostart.cpp:15-53`, Controller `autostart` property | `Platform::setAutostart` |
| Tray | `Controller.cpp:51-65` | `Platform::createTrayIcon` |
| Notifications | `Controller.cpp:685-690`, `748-754`, `797-814` | `Platform::notify` |
| Discovery roots | `discovery.cpp:880-887` (`defaultDiscoveryPaths`) | `Platform::discoveryPaths` |
| Email action | `discovery.cpp:865-875` (`xdg-email`) | `Platform::platformActionTargets` |
| Source-app stub | `SourceInfo.cpp:6-11` | Stays stub on Windows for MVP; optional P2+ `GetForegroundWindow` path |

`Controller` gets a `Platform *m_platform` injected at construction (from `main.cpp`). Linux implementation wraps today's behavior with minimal logic moves, not rewrites.

### New files (scaffold)

| File | Role |
|---|---|
| `src/platform/Platform.h` | Interface + `createPlatform()` |
| `src/platform/Platform.cpp` | Factory (`#ifdef Q_OS_WIN` / Linux) |
| `src/platform/linux/LinuxPlatform.{h,cpp}` | KDBusService, layer-shell, xdg-*, KStatusNotifierItem, KNotification, KWaylandExtras |
| `src/platform/windows/WindowsPlatform.{h,cpp}` | Mutex, QLocalServer, registry default-browser check, tray, toast |
| `src/platform/windows/WindowsOverlay.cpp` | HWND topmost, multi-monitor geometry, focus helpers |
| `src/platform/windows/WindowsRegistry.{h,cpp}` | URL handler registration helpers, App Paths scan, Run key autostart |
| `src/platform/windows/WindowsDiscovery.cpp` | `scanRegistryBrowsers()`, Windows `DiscoveryPaths` |
| `src/qml/windows/Picker.qml`, `Hold.qml` | Layer-shell-free overlay QML |
| `src/qml/linux/Picker.qml`, `Hold.qml` | Move current files here (or keep paths, see section 3) |
| `data/windows/lane-url.reg.in` | Template for http/https protocol registration |
| `cmake/PlatformWindows.cmake` | Optional: MSVC/Craft helper includes |

### CMake changes

`CMakeLists.txt` (root) and `src/CMakeLists.txt`:

- `if(WIN32)`: drop `LayerShellQt`, `KF6::DBusAddons`, `KF6::StatusNotifierItem`, `KF6::WindowSystem`, `Qt6::DBus`, `ecm_find_qmlmodule(org.kde.layershell)`.
- Keep Kirigami, I18n, CoreAddons, IconThemes, ColorScheme, qqc2-desktop-style. **[verified]** per `port-assessment.md`: NeoChat and other KF6 Kirigami apps ship on Windows via Craft; Lane is more Plasma-coupled than they are, but the Kirigami stack itself is precedented.
- Add `target_link_libraries(lane PRIVATE ...)` for `Qt6::Widgets` (tray), platform sources.
- Gate Linux-only install rules (`.desktop`, D-Bus service, systemd unit, notifyrc) behind `if(UNIX AND NOT APPLE)` or `if(LINUX)`.
- Windows install: copy `lane.exe`, QML, Qt/KF6 runtime (Craft or windeployqt), and run registry script post-install.

---

## 2. Phases

Effort is person-weeks for one experienced Qt developer. Tail QA (SmartScreen, multi-monitor edge cases) is additive.

### P0: Boot, IPC, default-browser handoff, minimal picker (3-4 pw)

**Goal:** Lane builds on Windows, stays resident, receives links, shows a picker, launches a browser.

**Demoable:** Install Lane, manually set it as default http/https handler in Windows Settings, click a link in another app, picker appears as a topmost frameless window, pick a target, browser opens. Second click while daemon runs does not spawn a second UI instance.

| Work item | Files |
|---|---|
| CMake Windows target | `CMakeLists.txt`, `src/CMakeLists.txt`, `cmake/PlatformWindows.cmake` (new) |
| Platform interface + Linux shim (move existing code) | `src/platform/*`, refactor `src/app/Controller.cpp`, `src/app/main.cpp`, `src/app/Autostart.cpp` (delete or thin to delegate) |
| Windows single instance: named mutex `Global\LaneSingleInstance` + `QLocalServer` on `\\.\pipe\lane` (or named pipe path under `%LOCALAPPDATA%\Lane\`) | `src/platform/windows/WindowsPlatform.cpp` |
| Second-process forward: CLI passes URL on argv; if mutex held, write length-prefixed message to pipe and `exit(0)` | `src/app/main.cpp`, `WindowsPlatform.cpp` |
| Registry URL handler: `HKCU\Software\Classes\Lane.URL\shell\open\command` → `"C:\...\lane.exe" "%1"`; ProgId `Lane` for http/https via installer reg file | `data/windows/lane-url.reg.in`, installer hook in P3 |
| Default-browser check: read `UserChoice` ProgId for `http` under `HKCU\Software\Microsoft\Windows\Shell\Associations\UrlAssociations\http\UserChoice` and compare to Lane's ProgId **[verified]** Microsoft documents UserChoice; programmatic *set* blocked since Win8 | `WindowsRegistry.cpp` |
| Default-browser UX: replace `makeDefaultBrowser()` xdg calls with `QDesktopServices::openUrl(QUrl("ms-settings:defaultapps"))` **[verified]** per assessment | `Controller.cpp:472-482`, `OverviewPage.qml:24-27` (button label: "Open Windows default app settings") |
| Minimal overlay: borderless `Qt::FramelessWindowHint \| Qt::WindowStaysOnTopHint`, `WS_EX_TOOLWINDOW` via `QWindow::fromWinId` or `Qt::ExpandedClientAreaHint` **[inference]**; full-screen on active monitor; no exclusive keyboard yet | `WindowsOverlay.cpp`, `src/qml/windows/Picker.qml` |
| Wire picker without layer-shell import | `src/CMakeLists.txt` (`qt_add_qml_module` per-platform file list), remove `configureLayerShell` calls on Windows |
| Stub discovery: hardcode Chrome/Firefox/Edge if found under App Paths | `WindowsDiscovery.cpp` |
| Drop `KCrash` on Windows or no-op | `main.cpp:67` |

**Not in P0:** hold HUD, rules UI polish, containers, keyboard-exclusive overlay, tray, notifications, autostart.

### P1: Discovery, rules, remembered, containers (2.5-3.5 pw)

**Goal:** Feature parity with Linux for routing logic and Lane differentiators that matter on Windows.

**Demoable:** Rules match and launch silently; Alt+A remember works; path-scoped memory ladder works; Firefox/Zen profiles and Chromium profiles discovered; containers launch via `ext+container:`; firefoxpwa sites appear.

| Work item | Files |
|---|---|
| Registry App Paths scan: `HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\App Paths\*.exe` | `WindowsDiscovery.cpp` |
| Gecko profiles: `%APPDATA%\Mozilla\Firefox\profiles.ini`, Zen under `%APPDATA%\zen` or install-specific paths **[inference]** mirror Linux `fingerprint()` tables | `discovery.cpp` (split `fingerprint()` OS tables) or `WindowsDiscovery.cpp` feeding same parsers |
| Chromium: `%LOCALAPPDATA%\Google\Chrome\User Data\Local State`, Edge, Brave, Vivaldi variants | same |
| firefoxpwa: `%APPDATA%\FirefoxPWA\config.json` **[verified]** per assessment | `discovery.cpp:799-851` path table |
| Start Menu / Uninstall registry for browsers missed by App Paths | `WindowsDiscovery.cpp` |
| `ext+container:` protocol registration: `HKCU\Software\Classes\ext+container\shell\open\command` → path to Firefox with args, or document that user must install the extension and register once via installer **[verified]** extension is cross-platform | `data/windows/ext-container.reg.in`, docs in installer |
| Reuse `discoverTargets()` parsers: inject `DiscoveryPaths` from `Platform::discoveryPaths()` | `discovery.cpp:880-887`, `Controller::reload()` |
| Rules + remembered + hold: no core changes expected; verify `router.cpp`, `destination.cpp`, settings QML | `src/qml/pages/RulesPage.qml`, tests |
| Email action: `mailto:?body=$url` via `QDesktopServices::openUrl` instead of `xdg-email` | `Platform::platformActionTargets()` |
| Unit test fixtures: Windows path fixtures in `tests/test_discovery.cpp` | `tests/fixtures/windows/` |

### P2: Overlay polish, tray, notifications, autostart, settings (2-3 pw)

**Goal:** Picker feels as fast and keyboard-driven as Linux; daemon is invisible between clicks.

**Demoable:** Type-to-filter works without clicking the filter field first; Escape/number keys reliable; hold HUD shows and cancels; tray icon opens settings; toast on open; autostart toggle works; settings window usable on Windows (no broken Kirigami assumptions).

| Work item | Files |
|---|---|
| Keyboard focus: `SetForegroundWindow`, `AllowSetForegroundWindow(ASFW_ANY)` on child PID after launch **[inference]**; optional global hotkey (`RegisterHotKey`) if focus steal fails when invoked from browser | `WindowsOverlay.cpp`, `Controller.cpp:720-728` (`showPicker` activate) |
| Multi-monitor: place overlay on screen containing cursor or invoking app **[inference]** | `WindowsOverlay.cpp` |
| Hold overlay: `src/qml/windows/Hold.qml`, wire `ensureHoldEngine` | `Controller.cpp:968-988` |
| Animations: match Linux timing (opacity on dim layer) | QML only |
| `QSystemTrayIcon` + context menu (Settings, Rediscover, Quit) | `WindowsPlatform.cpp`, remove `KStatusNotifierItem` include from Controller |
| Notifications: `QSystemTrayIcon::showMessage` for MVP; optional Win10 toast via `QDBus`/`WindowsRuntime` later **[inference]** | `WindowsPlatform.cpp` |
| Autostart: `HKCU\Software\Microsoft\Windows\CurrentVersion\Run`, value `Lane` → `"C:\...\lane.exe" --daemon` | `WindowsRegistry.cpp` |
| Settings window: verify Kirigami FormCard pages on Windows; fix any `org.kde.desktop` style gaps | `Settings.qml`, `OverviewPage.qml` (copy text: drop "Wayland" in autostart description) |
| `closeOnFocusLoss` behavior QA on Windows | `Controller.cpp`, QML |
| Source-app rules (optional): `GetForegroundWindow` + `GetWindowThreadProcessId` in `SourceInfo.cpp` | `SourceInfo.cpp`, `SourceInfo_windows.cpp` |

### P3: Packaging, signing, updates (1.5-2.5 pw)

**Goal:** A stranger can download, install, and trust Lane on Windows 10/11 x64.

**Demoable:** Signed installer completes; SmartScreen does not block (or shows publisher name); uninstall removes registry handlers; in-app update check still works (existing `UpdateChecker`).

| Work item | Files |
|---|---|
| **MSIX vs NSIS tradeoff** (see below) | `packaging/windows/` (new dir) |
| Authenticode signing in CI | GitHub Actions `windows-latest`, secret cert |
| Installer registers/unregisters URL handlers and `ext+container` | `.reg` or NSIS `WriteRegStr` |
| `windeployqt` or Craft runtime bundling | CI script |
| Auto-update story: keep GitHub Releases + in-app notify (no Sparkle on Windows); optional winget manifest **[inference]** | `UpdateChecker.cpp` unchanged; `packaging/winget/lane.yaml` |
| EULA / PolyForm NC shown in installer | `COMMERCIAL.md` reference |

**MSIX vs NSIS**

| | MSIX | NSIS (or Inno Setup) |
|---|---|---|
| SmartScreen | Still needs Authenticode; MSIX adds store trust path **[inference]** | Standard .exe installer; reputation builds on download volume |
| URL protocol registration | Package manifest `uap:Protocol` **[verified]** MSIX supports custom protocols | Registry writes in install section |
| Autostart | StartupTask extension or Run key (less clean in MSIX) **[inference]** | Run key straightforward |
| KDE/Qt deps | Large package; VCLibs/redist **[inference]** | Folder + PATH, familiar for Browser Tamer users |
| Recommendation | Defer to P4 unless Store listing is a goal | **Default for P3** [inference]: matches competitor distribution, simpler protocol registration, Andy controls release cadence |

**Signing cost:** Authenticode OV cert roughly $200-400/year **[verified]** typical market range cited in assessment; EV reduces SmartScreen friction faster **[inference]**.

**SmartScreen cold-start:** New cert = "Unknown publisher" warnings until reputation accumulates (weeks, download volume dependent) **[verified]** Microsoft SmartScreen behavior.

### P4 (defer): ARM64, Microsoft Store

| Item | Notes |
|---|---|
| ARM64 Windows | Build matrix entry; Qt/KF6 ARM64 via Craft **[inference]**; QA on Surface/ARM hardware |
| Store listing | Needs MSIX, age ratings, PolyForm NC vs Store policy review **[inference]** |
| winget publish | Low cost after P3 NSIS exists |

**Effort if pursued later:** 1-2 pw ARM64, 2-3 pw Store pipeline.

---

## 3. Linux-only vs swapped

| Linux today | Windows swap | Recommendation |
|---|---|---|
| `org.kde.layershell` QML import | None; topmost `QWindow` | **Per-platform QML files** in `qt_add_qml_module`: `qml/linux/Picker.qml` vs `qml/windows/Picker.qml`. CMake selects the list by `WIN32`. Avoid runtime fallback in one file: layer-shell import fails module load on Windows if present. |
| `LayerShellQt::Window` in C++ | `WindowsOverlay::configure()` | Linux keeps `configureLayerShell`; Windows sets flags + HWND ex-style |
| `KStatusNotifierItem` | `QSystemTrayIcon` | Platform factory |
| `KDBusService` | Mutex + `QLocalServer` | Platform |
| `KNotification` + notifyrc | Tray message or toast | Platform |
| `KWaylandExtras::xdgActivationToken` | `AllowSetForegroundWindow` + attach thread input **[inference]** | Platform `requestActivationToken` returns empty string on Windows; `applyActivationForChildProcess` calls Win32 APIs on launched PID |
| `XDG_ACTIVATION_TOKEN` env in child | No env; ASFW on parent before spawn **[inference]** | `launcher.cpp` delegates to Platform |
| `xdg-settings` / `xdg-mime` | Registry read + Settings deep link | Platform |
| XDG autostart `.desktop` | Run key | Platform |
| `scanDesktopFiles()` | Registry App Paths + Start Menu | `WindowsDiscovery.cpp` feeding shared parsers |
| Flatpak branches in `discovery.cpp` | Skip on Windows | `#ifdef` or engine check |
| `xdg-email` | `mailto:` with body | Platform action targets |
| systemd user unit | N/A | Not installed |
| D-Bus `.service` file | N/A | Not installed |

**QML shared code:** `Settings.qml` and inner pages stay shared; only `Picker.qml` and `Hold.qml` fork.

---

## 4. Risks

| Risk | Severity | Mitigation |
|---|---|---|
| **Focus stealing / keyboard grab** | **Highest** | Windows restricts `SetForegroundWindow` unless the calling thread owns the foreground window or the user initiated the chain. Lane is invoked as the default browser, so the first activation often works; reliability of type-to-filter without clicking the field is the open question. Research: `AllowSetForegroundWindow`, `AttachThreadInput`, UIPI, `WS_EX_NOACTIVATE` on overlay vs `WS_EX_TOPMOST`. Fallback: global hotkey to summon picker, or accept click-to-focus on filter field for v1. |
| Second-process spawn per click | High until P0 IPC ships | Mutex + pipe must land in first Windows milestone |
| Default-browser onboarding friction | Medium | Clear Settings copy; cannot one-click set since Win8 **[verified]**; consider first-run wizard |
| SmartScreen / signing cost | Medium | Budget cert; expect warning period; EV cert optional |
| No Windows dev box for Andy | Medium | **[open question]** GitHub Actions `windows-latest` for build; manual QA on real hardware or VM |
| CI: Kirigami on Windows | Medium | Craft blueprint (KDE) or vcpkg KF6 **[inference]**; verify against NeoChat Craft recipe **[verified]** precedent in assessment |
| `ext+container` registration | Low | Document manual step; bundle reg snippet in installer |
| PolyForm NC commercial use | Low (legal, not technical) | Andy can sell his own build; blocks competitors from forking commercially |
| Tray on Win11 "hidden icons" | Low | Document tray location; optional "open settings" shortcut |

---

## 5. MVP definition

**Smallest shippable Windows build that competes with Browser Tamer:**

Rules engine + profile/PWA discovery + picker + path-scoped remembered destinations + default-browser proxy loop. User can install, set Lane as default via Settings, click links, get a fast picker, and have silent routing for remembered hosts and explicit rules.

**Explicitly post-MVP for launch:** hold HUD (P2), containers (P1 if ready, else fast-follow), source-app rules, curated tracking-param list, Plasma-only features (KActivities).

Two-line summary: *Ship when a Windows user can make Lane the default http/https handler, click a link, and have rules + remembered paths route silently while everything else gets the keyboard picker with profile-aware targets.*

---

## 6. Open questions for Andy

1. **Windows dev machine?** Local VM, physical box, or CI-only + borrowed QA? Blocks overlay focus tuning.
2. **Signing cert budget?** OV vs EV; who holds the cert (personal vs Boundless IT Systems)?
3. **MSIX vs NSIS?** Plan defaults NSIS unless Store distribution is a near-term goal.
4. **Commercial licensing?** PolyForm NC allows personal use free; Andy can sell Lane commercially per `COMMERCIAL.md`. Is a paid Windows SKU intended, or free + donation?
5. **ARM64 priority?** Any Surface/ARM users in target audience, or defer P4?
6. **Container extension:** Bundle Firefox extension install instructions, or assume power users already have it?
7. **Competitive positioning:** Lead with path-scoped memory + hold HUD in marketing, or match Browser Tamer feature checklist first?

---

## 7. CI sketch

```yaml
# .github/workflows/windows.yml (new, P0)
runs-on: windows-latest
steps:
  - install Qt 6.6+ and KF6 via Craft OR prebuilt KDE/Craft cache
  - cmake -B build -DCMAKE_BUILD_TYPE=Release
  - cmake --build build
  - ctest --test-dir build  # core tests; discovery fixtures gated
  - windeployqt build/lane.exe
```

**[inference]** Start with Craft `craftmaster` KDE blueprint; fall back to manual Qt + vcpkg KF6 if Craft setup time blocks P0.

---

## 8. Test plan (per phase)

| Phase | Automated | Manual |
|---|---|---|
| P0 | `test_router`, `test_matcher`, `test_pipeline` (unchanged) | Click link from Edge, Notepad URL, Run dialog; second instance IPC |
| P1 | `test_discovery` with Windows fixtures | Profile count matches Firefox Settings; container row launches correct identity |
| P2 | None required for tray | Keyboard-only picker session; hold cancel/confirm; autostart reboot |
| P3 | Installer smoke on clean VM | SmartScreen path; uninstall leaves no default handler |

---

## Report summary

| Phase | Effort (pw) | Demoable end state |
|---|---|---|
| P0 | 3-4 | Default handler + IPC + minimal picker opens browser |
| P1 | 2.5-3.5 | Rules, remembered, discovery, containers |
| P2 | 2-3 | Keyboard overlay, hold, tray, notifications, autostart |
| P3 | 1.5-2.5 | Signed installer, production deploy |
| P4 | defer | ARM64, Store |

**Single biggest technical risk:** Windows focus-stealing restrictions may prevent reliable keyboard-exclusive picker behavior without `AllowSetForegroundWindow`/`AttachThreadInput` gymnastics or a global-hotkey fallback.

**MVP (2 lines):** Rules + profiles + picker + path-scoped remembered destinations on Windows, with the default-browser IPC loop working. Hold HUD and containers can land in P1/P2 without blocking first release.
