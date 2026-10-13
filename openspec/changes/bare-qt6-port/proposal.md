# Proposal: Bare Qt6 Port (de-KF6 the app layer)

## Why

Lane ships as a KDE Plasma app with KF6 woven through the app layer: `KDBusService` for single instance, `KStatusNotifierItem` for tray, `KNotification` for toasts, `KCrash`, `KAboutData`, `KWindowSystem`/`KWaylandExtras` for activation, `KLocalizedString` for i18n. That makes the daemon unbuildable anywhere KF6 does not ship cleanly — Windows, macOS — and pins distribution to KDE's toolchain.

The routing brain (`src/core/`) is already pure Qt6 and proven portable: it builds and passes all tests on macOS arm64 today. The KF6 surface in `src/app/` is shallow (~20 call sites) and every one has a stock Qt6 or thin D-Bus replacement. Removing KF6 converts the port from "impossible without Craft" to "write per-OS backend files."

## What Changes

- **Single instance**: `KDBusService` replaced by `QLocalServer`/`QLocalSocket` for the unique-instance handoff, plus a small `org.freedesktop.Application`-compatible `QDBusObject` on Linux so `DBusActivatable=true` desktop launch keeps working.
- **Tray**: `KStatusNotifierItem` → `QSystemTrayIcon` (passive icon, activate opens settings — the only two features used today).
- **Notifications**: `KNotification` → new `PlatformNotify` helper. Linux talks to `org.freedesktop.Notifications` over `QDBus` and keeps action buttons (the watchdog "Restore Lane" action survives). Other platforms degrade to `QSystemTrayIcon::showMessage` or a settings-page banner.
- **Crash handling**: `KCrash` removed; replaced by a `qInstallMessageHandler` + `std::set_terminate` crash log written under `QStandardPaths::AppDataLocation`.
- **About**: `KAboutData` removed; application metadata set directly on `QCoreApplication`; About UI becomes a small QML card reading the same values (also fixes the missing PolyForm license text — `KAboutLicense::Custom` never rendered it).
- **Activation**: `KWindowSystem`/`KWaylandExtras` xdg-activation moved behind a `platform/` seam; Linux keeps the existing Wayland token path guarded by feature check, other platforms get `requestActivate()`.
- **i18n**: `KLocalizedString`/`i18n()` → `QObject::tr()`/`qsTr()`.
- **Build**: `find_package(KF6 ...)` reduced to what the platform seam still needs per-OS; LayerShellQt stays (plain Qt wrapper, not KF6). ECM stays on Linux for install dirs and the version header.
- **Platform seam**: `src/app/platform/` introduced with a small virtual interface (singleInstance, defaultBrowser, autostart, tray, notify, overlay, sourceInfo) and `platform_linux.cpp` implementing today's behavior.

## Capabilities

### New Capabilities
- `cross-platform`: Lane builds and runs its full app feature set without KDE Frameworks; per-OS behavior is supplied by a platform backend, not compile-time KDE assumptions.

### Modified Capabilities
- `lane`: single-instance handoff, tray, notification actions, crash handling, and activation are re-specified against Qt primitives instead of KDE services. User-visible behavior is unchanged on Linux; `DBusActivatable` launch continues to work.

## Impact

- `src/app/`: `main.cpp`, `Controller.cpp/h`, `Autostart.cpp`, `DefaultBrowserWatcher` wiring — KF6 includes removed.
- `src/app/platform/`: new directory and interface.
- `CMakeLists.txt` / `src/CMakeLists.txt`: KF6 `find_package`/`target_link_libraries` lists shrink on Linux and disappear elsewhere.
- `docs/plan-windows-port.md`, `AGENTS.md`, `README.md`: build instructions and dependency lists updated.
- No config schema changes. No behavior changes on Linux beyond crash-dialog → crash-log.
- **BREAKING** (build only): building Lane now requires only Qt6 + LayerShellQt on Linux; KF6 development packages are no longer needed.
