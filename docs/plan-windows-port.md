# Windows port plan

Status: draft, pre-work. Audit basis: full dependency inventory of
`src/`, `data/`, and `CMakeLists.txt` on main (2026-10-06).

## What the port actually is

Lane's routing brain is already portable. `src/core/` is pure Qt —
URL safety, rule matching, pipeline, router, destination ladder,
launcher argv safety, config JSON, unshorten — with zero OS calls.
All QML is pure QML: Kirigami + kirigami-addons have no native backends,
and KDE already ships Kirigami apps on Windows via Craft.

The port is therefore not a rewrite. It is five platform seams:

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

## The one decision to make first

**Keep KF6 via KDE Craft toolchain, or strip to bare Qt6?**

- **Keep KF6 (Craft)**: KNotifications, KCrash, KIconThemes, KColorScheme
  survive as links; only KDBusService and KStatusNotifierItem get swapped
  (QLocalServer, QSystemTrayIcon). Smallest code diff. Cost: build system
  is tied to KDE's Windows toolchain.
- **Strip KF6**: every KF6 dependency becomes a real rewrite — toasts,
  crash handler, tray, metadata. More work up front, zero KDE dependency
  in the shipped binary, easier distribution (single Qt installer).

Recommendation: **Craft/KF6 for the first working port**. Proving the
product on Windows matters more than packaging purity; a bare-Qt fork
can come later if distribution demands it. Flag: the live state of
KF6::Notifications' Windows backend and KCrash's MinGW/MSVC backend
were not verified — do one Craft test build before committing.

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
  platform_linux.cpp   // current KDBus/LayerShell/XDG impl, moved as-is
  platform_windows.cpp // QLocalServer + registry + Win32 impl
```

`Controller` keeps one code path; each virtual has a Linux impl that is
literally the current code. Discovery gets a `DesktopApp`-shaped
producer per platform (registry enumerator on Windows) feeding the
unchanged `fingerprint()`/`geckoProfiles()`/`chromiumProfiles()`.

## Milestones

1. **Build skeleton** — CMake `if(WIN32)` guards for LayerShellQt /
   DBusAddons / StatusNotifierItem; Craft toolchain build that links.
   No behavior yet.
2. **Single instance + settings** — QLocalServer handoff, `--settings`
   window renders on Windows, config round-trips at `%APPDATA%\lane`.
3. **Discovery** — registry browser enumeration + Gecko/Chromium path
   tables; `lane --list` shows real Windows browsers/profiles.
4. **Picker + launch** — overlay shows on URL handoff, launches browsers
   via `QProcess::startDetached` (already portable).
5. **Default-browser registration + takeover watch** — registry
   capabilities, `ms-settings` deep link, `RegNotifyChangeKeyValue`
   watcher.
6. **Autostart + tray** — Run-key toggle, `QSystemTrayIcon`.
7. **Polish** — crash handler decision, notifications fallback,
   installer (MSIX or Inno), update feed per-OS.

Out of scope: mailto/PDF (unchanged — never intercepted), browser
extensions, macOS.

## Risks

- Craft/KF6-on-Windows viability for Notifications/KCrash — verify
  before milestone 1 is called done.
- Windows overlay keyboard-focus behavior differs from layer-shell
  exclusive grabs; `WindowStaysOnTopHint` + `requestActivate` is close
  but needs real testing on Windows 11.
- Zen on Windows profile location unverified.
- Per-Activity UI stays permanently empty on Windows — consider hiding
  it when `HAVE_PLASMA_ACTIVITIES` is absent (also a Linux cleanup).
