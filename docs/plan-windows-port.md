# Windows port plan

Status: decided — bare Qt6, no KF6. Audit basis: full dependency
inventory of `src/`, `data/`, and `CMakeLists.txt` on main
(2026-10-06), updated after the Kirigami/FormCard usage count.

## What the port actually is

Lane's routing brain is already portable. `src/core/` is pure Qt —
URL safety, rule matching, pipeline, router, destination ladder,
launcher argv safety, config JSON, unshorten — with zero OS calls.

The C++ KF6 surface is shallow: five `KNotification` calls, one
`KStatusNotifierItem`, `KDBusService`, `KAboutData`, `KCrash`,
`KWindowSystem`/`KWaylandExtras` for activation, `KLocalizedString`.
Every one has a Qt6 replacement (table below).

The one genuinely large piece is the QML. `src/qml/` is built on
Kirigami + kirigami-addons FormCard: ~80 FormCard delegates across 4
settings pages, `Kirigami.Theme` at 63 sites, `Kirigami.Units` at 22,
`Kirigami.ShadowedRectangle`, `Kirigami.Icon`, and
`Kirigami.ListItemDragHandle` for the Targets page drag-reorder. Bare
Qt6 means a real UI port to Qt Quick Controls — not a find/replace.

## Decision: bare Qt6, and why

KF6-via-Craft was the alternative. It keeps the code diff smallest but
ships KDE frameworks inside a Windows installer, ties the build to
KDE's toolchain, and still doesn't solve the QML question (Kirigami
works on Windows but the app then looks like a KDE app everywhere,
forever).

Bare Qt6 wins on results, not effort:

- One dependency tree: stock Qt6 + Qt6 declarative, installable via
  windeployqt or a static build. No Craft, no ECM in the shipped path.
- Porting the UI to Qt Quick Controls once gives **one QML codebase for
  both platforms** — on Plasma, `qqc2-desktop-style` renders it native
  anyway, so Linux loses almost nothing visually.
- Every KF6 replacement below is either a stock Qt class or a thin
  D-Bus call we already depend on.

### KF6 → bare Qt6 mapping

| KF6 today | Bare Qt6 replacement | Notes |
|---|---|---|
| `KDBusService` (unique instance + activation) | `QLocalServer`/`QLocalSocket` for the handoff | Keep a tiny `org.freedesktop.Application`-compatible D-Bus object on Linux so `DBusActivatable=true` keeps working; Windows is QLocalServer only |
| `KStatusNotifierItem` (tray) | `QSystemTrayIcon` | SNI has more features (context menu via D-Bus); current use is a passive icon + activate → settings, which QSystemTrayIcon covers on both OSes |
| `KNotification` (5 sites) | Linux: `org.freedesktop.Notifications` via `QDBus` (actions supported — the "restore" button survives); Windows: `QSystemTrayIcon::showMessage` or WinRT toast | WinRT toast with an action button is ~100 lines of WinRT/COM; acceptable fallback is a settings-page banner |
| `KCrash` | Drop, or `QMessageLogContext` + `std::set_terminate` writing a crash log | KCrash's value is the KDE crash dialog; a log file is enough for a single-binary app |
| `KAboutData` | `QCoreApplication` metadata + a hand-rolled About page | The current About is already custom-license; a QML About card is ~40 lines |
| `KLocalizedString` / `i18n` | `qsTr()`/`QTranslator` | QML already avoids `i18n()` (0 hits); C++ has ~1 call site |
| `KWindowSystem`/`KWaylandExtras` (activation token) | Linux: keep `KWindowSystem` optional via `#ifdef`, or accept `requestActivate()` on X11 path; Windows: `SetForegroundWindow` rules allow it from a foreground process | Activation polish, not correctness |
| `KIconThemes`/`KColorScheme` | `QIcon::fromTheme` (Qt honors Freedesktop icon themes on Linux), `QPalette` | Already partially used |
| `KConfigCore` | Already unused at runtime — config is our own JSON | Drop the link |

LayerShellQt stays **Linux-only**, behind `if(UNIX AND NOT APPLE)` —
it is a plain Qt wrapper for wlr-layer-shell, no KF6 required, and the
existing X11/Windows overlay path is already `FramelessWindowHint |
WindowStaysOnTopHint`.

## The five platform seams (unchanged)

| Seam | Linux today | Windows equivalent | Effort |
|---|---|---|---|
| Single instance + URL relay | KDBusService / D-Bus | `QLocalServer`/`QLocalSocket` at `main()` | Medium |
| Default browser set/get | `xdg-settings` / `xdg-mime` | Registry reads + `ms-settings:defaultapps` user step | Medium + UX gap |
| Browser discovery front end | `.desktop` / `mimeapps.list` parsing | `HKLM/HKCU\...\Clients\StartMenuInternet` + `shell\open\command` | Large |
| Overlay windowing | LayerShellQt (Wayland) | Plain `Qt::FramelessWindowHint \| Qt::WindowStaysOnTopHint` (existing X11 fallback already correct) | Small |
| Autostart | XDG autostart `.desktop` | `HKCU\...\Run\Lane` value or Startup-folder `.lnk` | Small |

Plus two drops and one upgrade:

- **Drop**: Plasma Activities (already compile-time optional,
  `HAVE_PLASMA_ACTIVITIES`; no Windows concept).
- **Drop**: `KWaylandExtras` xdg-activation token negotiation. Windows
  focus rules allow a foreground process to activate a spawned window;
  the whole subsystem becomes a plain `requestActivate()`.
- **Upgrade opportunity**: `SourceInfo.cpp` is a stub on Wayland
  (no portable active-window PID). On Windows `GetForegroundWindow` +
  `QueryFullProcessImageNameW` are stable APIs — per-source rules
  (ProcessName/WindowTitle match) can actually work there.

## The one real functionality gap

Windows 10+ `UserChoice` is hash-protected. Lane **cannot silently set
itself as default browser** — that is a deliberate anti-hijack design,
not an API oversight. The honest UX:

1. Lane registers itself as a browser candidate
   (`HKLM/HKCU\SOFTWARE\Clients\StartMenuInternet\Lane\Capabilities`,
   `RegisteredApplications`, ProgId with `shell\open\command`).
2. "Use Lane as default browser" opens `ms-settings:defaultapps`
   (or `ms-settings:defaultapps?registeredAppUser=Lane`) and shows a
   short inline guide instead of flipping a switch.
3. Takeover detection watches `UrlAssociations\http\UserChoice` via
   `RegNotifyChangeKeyValue` (through `QWinEventNotifier`) — the
   DefaultBrowserWatcher contract (`mimeappsChanged()` signal +
   before/after compare in Controller) survives unchanged.

This is the same promise as today, one user step heavier. It matches
what every Windows browser chooser (Browser Tamer, Picky) does.

## Discovery on Windows

The Linux front end parses `.desktop` files. On Windows the equivalent
discovery is registry enumeration:

- Browsers: `HKLM\SOFTWARE\Clients\StartMenuInternet\*` and
  `HKCU\Software\Clients\StartMenuInternet\*`, each `shell\open\command`
  gives the exe. No `Exec=` field-code/env/flatpak grammar exists —
  ~330 lines of `discovery.cpp`'s Exec tokenizer become unreachable on
  Windows builds.
- Gecko profiles (Firefox/Zen/LibreWolf/Floorp/Waterfox): same
  `profiles.ini` format, path table changes to `%APPDATA%` variants
  (Firefox `%APPDATA%\Mozilla\Firefox`, Zen `%APPDATA%\zen` — verify
  against a real install). `bestGeckoDataDir`/`geckoProfiles`/container
  parsing (~320 lines) reuse verbatim.
- Chromium profiles (Chrome/Brave/Edge/Vivaldi/Opera): same
  `Local State` JSON `profile.info_cache`, paths move to
  `%LOCALAPPDATA%\<Brand>\User Data` (Opera is the outlier:
  `%APPDATA%\Opera Software\Opera Stable`). `chromiumProfiles()`
  unchanged.
- `containers.json`/`extensions.json`: profile-relative, no change.
- firefoxpwa: cross-platform Rust CLI; config at
  `%APPDATA%\firefoxpwa\config.json`. Path swap only.
- Flatpak path translation: dead code under `#ifdef` — prune.

## Abstraction shape

Keep `src/core/` shared. Introduce a small platform seam in `src/app/`:

```
src/app/platform/
  platform.h           // Virtuals: singleInstance, defaultBrowser,
                       // autostart, tray, notify, overlayFlags,
                       // sourceInfo, discoveryRoots
  platform_linux.cpp   // current behavior, minus KF6 (QDBus, QLocalServer)
  platform_windows.cpp // QLocalServer + registry + Win32 impl
```

`Controller` keeps one code path; each virtual has a Linux impl that is
the current logic re-expressed on Qt APIs. Discovery gets a
`DesktopApp`-shaped producer per platform (registry enumerator on
Windows) feeding the unchanged `fingerprint()`/`geckoProfiles()`/
`chromiumProfiles()`.

## UI port approach (the big one)

One QML codebase on Qt Quick Controls, shared by both platforms:

- **Settings pages**: replace FormCard delegates with a small local
  `ui/` module — `FormRow`, `SwitchRow`, `ComboRow`, `ButtonRow`,
  `TextRow` — each ~30-60 lines of QQC2. ~80 call sites, but they are
  mechanical: FormCard delegates already take `text:`/`description:`/
  `checked:` shapes that map 1:1.
- **`Kirigami.Theme`** → `ApplicationWindow.palette` /
  `Material`/`Fluent`/`qqc2-desktop-style` theme colors. The 63 uses
  are mostly text color and highlight — sed-able, then hand-checked.
- **`Kirigami.Units`** → fixed dp constants or `Application.font`
  metrics.
- **`Kirigami.ShadowedRectangle`** → `Rectangle` + `DropShadow`
  (Qt6 `MultiEffect` or `layer.effect`).
- **`Kirigami.Icon`** → `IconImage` (Qt 6.8+ supports themed icons on
  Linux) or `Image` + `QIcon::fromTheme` provider.
- **`ListItemDragHandle`** → no QQC2 equivalent; port the Kirigami
  source pattern (a MouseArea-driven drag that moves the delegate in
  the ListView) — self-contained, ~100 lines.
- Style selection: `Fusion` everywhere, or `qqc2-desktop-style` on
  Linux (Plasma-native look) + `Fluent`/`Fusion` on Windows. Decide by
  screenshot comparison in milestone 2 — pick whichever keeps Plasma
  look acceptable; do not ship two QML trees.

## Milestones

1. **De-KF6 the C++** — swap every KF6 call site per the mapping table
   while still on Linux; CI + `ctest` stay green the whole time. This
   is the largest code-touching milestone but is fully testable on
   Linux with zero Windows access.
2. **UI port to Qt Quick Controls** — build the `ui/` delegate module,
   migrate the 7 QML files, screenshot-compare settings/picker/hold
   against current Kirigami renders, keep Plasma look via
   `qqc2-desktop-style` if it holds up.
3. **Windows build skeleton** — CMake `if(WIN32)` guards for
   LayerShellQt/DBus paths; MSVC or llvm-mingw build that links; config
   round-trips at `%APPDATA%\lane`.
4. **Single instance + settings** — QLocalServer handoff, `--settings`
   window renders on Windows.
5. **Discovery** — registry browser enumeration + Gecko/Chromium path
   tables; `lane --list` shows real Windows browsers/profiles.
6. **Picker + launch** — overlay shows on URL handoff, launches browsers
   via `QProcess::startDetached` (already portable).
7. **Default-browser registration + takeover watch** — registry
   capabilities, `ms-settings` deep link, `RegNotifyChangeKeyValue`
   watcher.
8. **Autostart + tray + notifications** — Run-key toggle,
   `QSystemTrayIcon`, toast path.
9. **Polish** — crash log decision, installer (MSIX or Inno),
   update feed per-OS.

Out of scope: mailto/PDF (unchanged — never intercepted), browser
extensions, macOS.

## Risks

- **QQC2 settings pages will not be pixel-identical** to FormCard.
  Budget real design time in milestone 2; the risk is a functional but
  bland settings window. Mitigation: build the `ui/` delegate module to
  mimic FormCard's grouped-card look.
- **`ListItemDragHandle` port** — Kirigami's implementation is
  battle-tested; a hand-rolled replacement must handle auto-scroll,
  section boundaries, and the kind+incognito sibling rules. Tests on
  `moveIdAmongSiblings` already cover the model side.
- **Notification action on Windows** — WinRT toast-with-button is the
  only way to keep the "restore default" action; if that proves fragile,
  degrade to a banner inside settings (flagged, acceptable).
- **Single-instance without KDBusService** — `DBusActivatable=true`
  needs either a kept `org.freedesktop.Application` D-Bus object or a
  `.desktop` rewrite to plain `Exec=`. Verify gtk-launch still works
  end-to-end; this bit us before.
- Windows overlay keyboard-focus behavior differs from layer-shell
  exclusive grabs; `WindowStaysOnTopHint` + `requestActivate` is close
  but needs real testing on Windows 11.
- Zen on Windows profile location unverified.
- Per-Activity UI stays permanently empty on Windows — hide it when
  `HAVE_PLASMA_ACTIVITIES` is absent (also a Linux cleanup).
