# Proposal: Bare Qt6 Port (de-KF6 the app layer)

## Why

Lane ships as a KDE Plasma app with KF6 woven through the app layer: `KDBusService` for single instance, `KStatusNotifierItem` for tray, `KNotification` for toasts, `KCrash`, `KAboutData`, `KWaylandExtras` for activation-token handoff, `KLocalizedString` for i18n. That makes the daemon unbuildable anywhere KF6 does not ship cleanly — Windows, macOS — and pins distribution to KDE's toolchain.

The routing brain (`src/core/`) is already pure Qt6 and proven portable: it builds and passes all tests on macOS arm64 today. Every KF6 use in `src/app/` has a stock Qt6 or thin D-Bus replacement; the two that don't (`KWaylandExtras`, `KActivities`) resolve to "optional link when present" and "Linux-only Plasma::Activities (not KF6)".

## What Changes

- **Single instance**: `KDBusService` → `QLocalServer`/`QLocalSocket` on a `$XDG_RUNTIME_DIR` socket (`UserAccessOption`, stale-socket recovery) for the unique-instance + argv/token handoff, plus a `QDBusAbstractAdaptor` on `/app/lane/Lane` **and** `/MainApplication` exposing `org.freedesktop.Application` (`Activate`/`Open`/`ActivateAction`) and `org.kde.KDBusService.CommandLine` — `DBusActivatable=true` launch, `gtk-launch`, and systemd `Type=dbus` readiness all preserved, including the `platform_data` activation-token relay.
- **Tray**: `KStatusNotifierItem` → `QSystemTrayIcon` with exact-parity menu (Settings, Rediscover browsers; no Quit). Passive status has no Qt equivalent — icon becomes always-visible.
- **Notifications**: `KNotification` → `org.freedesktop.Notifications` over `QDBus` via a `platform()->notify()` seam, preserving action buttons (watchdog "Restore Lane"), per-site icons, and the `desktop-entry` hint. Service-absent degradation: no-op + the Overview restore path.
- **Crash handling**: `KCrash` → `qInstallMessageHandler` ring buffer + `std::set_terminate` + fatal-signal handlers writing a crash log under the app data dir.
- **Metadata**: `KAboutData` + `KLocalizedString` → `QCoreApplication` setters (already present) + `tr()`. No About page — Overview already shows version + PolyForm notice.
- **Activation token**: `KWaylandExtras` → `KF6::WindowSystem` as an **optional** link (`HAVE_KWAYLANDEXTRAS`); absent → immediate empty token (300ms fallback path already exists).
- **Platform seam**: `src/app/platform/` — `Platform` interface (notify, autostart, `requestActivationToken` async producer) + `platform_linux.cpp`.
- **Build**: all KF6 link entries removed except optional WindowSystem; dead links (`ConfigCore`, `IconThemes`, `ColorScheme`) confirmed zero-call-site; `notifyrc` file deleted; CI/packaging/docs dependency lists shrink — CI becomes the KF6-free proof.

## Capabilities

### New Capabilities
- `cross-platform`: Lane builds and runs its full app without required KDE Frameworks; per-OS behavior comes from a platform backend, including single-instance transports, notification actions, crash logging, and activation-token relay across every handoff path.

### Modified Capabilities
- `lane`: adds spec'd requirements for tray presence/menu, application metadata, and crash log (previously unspecced behaviors now pinned as the implementation swaps).

## Impact

- `src/app/`: `main.cpp`, `Controller.cpp/h` — all KF6 includes/callsites replaced; new `SingleInstance`, `DBusActivation`, `CrashLog`, `platform/` files.
- `CMakeLists.txt`, `src/CMakeLists.txt`, `.github/workflows/ci.yml`, `packaging/PKGBUILD`, `CONTRIBUTING.md`, `CLAUDE.md`, `README.md` — dependency lists updated.
- `data/app.lane.Lane.notifyrc` — deleted.
- No config schema changes. User-visible deltas on Linux: tray icon always-visible (no Passive), crash dialog → crash log file, everything else identical.
- **BREAKING** (build only): building Lane requires Qt6 + LayerShellQt on Linux; KF6 dev packages optional (kwindowsystem only, for the activation token).
