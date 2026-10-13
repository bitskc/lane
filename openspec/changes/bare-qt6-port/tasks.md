# Tasks: bare-qt6-port

Ordered; each task leaves the build green on Linux.

## 1. Platform seam scaffolding
- [ ] Create `src/app/platform/platform.h` with the `Platform` interface (notify, autostartSet/Enabled, raiseWindow) and `platform()` accessor returning a singleton.
- [ ] Create `src/app/platform/platform_linux.cpp` implementing the interface with stub bodies that call through to the existing code paths (initially thin forwarding so nothing changes behavior yet).
- [ ] Register `platform_linux.cpp` in `src/CMakeLists.txt` under `LANE_KDE_DESKTOP`.

## 2. Wayland activation spike (unblocks decision on KF6::WindowSystem)
- [ ] Check whether `QNativeInterface::QWaylandApplication` exposes enough to request an xdg-activation token without `KWaylandExtras`/`KWindowSystem` (read Qt 6.11 headers locally: `/usr/include/qt6/QtGui/*/qpa/qplatformnativeinterface.h`, `QWaylandApplication` native interface).
- [ ] If feasible: implement `raiseWindow` with token negotiation; if not: keep `KF6::WindowSystem` linked as the single documented exception and record it in design.md.

## 3. Single instance: QLocalServer + freedesktop Application object
- [ ] Add `src/app/SingleInstance.cpp/h`: `QLocalServer` listener + `QLocalSocket` forwarding (payload = argv list as JSON), integrated in `main.cpp` before `Controller` construction (preserves the audit ordering fix).
- [ ] Implement `org.freedesktop.Application` D-Bus adaptor (`Activate`, `Open`, `ActivateAction`) on `app.lane.Lane` via `QDBusAbstractAdaptor` + `QDBusConnection::sessionBus().registerObject/registerService`.
- [ ] Remove `KDBusService` from `main.cpp` and `KF6::DBusAddons` from link list.
- [ ] Verify: `qdbus6 app.lane.Lane` introspection lists `Open`; `gtk-launch app.lane.Lane` with a URL routes it; duplicate `lane <url>` forwards instead of respawning.

## 4. Tray → QSystemTrayIcon
- [ ] Replace `KStatusNotifierItem` member in `Controller` with `QSystemTrayIcon` (icon `QIcon::fromTheme("app.lane.Lane")`, tooltip "Lane", `Activated` → open settings, minimal context menu: Settings, Quit).
- [ ] Drop `KF6::StatusNotifierItem` from link list.

## 5. Notifications → platform()->notify()
- [ ] Implement `Platform::notify` on Linux via `org.freedesktop.Notifications` `Notify` D-Bus call incl. actions array; dispatch `ActionInvoked` by notification id to the stored action lambda; fallback to `QSystemTrayIcon::showMessage` when the bus call fails.
- [ ] Migrate the 5 `KNotification` call sites in `Controller.cpp` (browser-takeover w/ restore action, save-failed, launch-failed, opened ×2).
- [ ] Drop `KF6::Notifications` and `data/app.lane.Lane.notifyrc` install (delete file; the event-id scheme was KNotify-specific).
- [ ] Verify: trigger a launch-failure path or send a test notification; confirm action button appears on Plasma.

## 6. Crash log
- [ ] `src/app/CrashLog.cpp/h`: `qInstallMessageHandler` ring buffer (~200 entries) + `std::set_terminate`; flush to `QStandardPaths::AppDataLocation/crash-<epoch>.log`.
- [ ] Remove `KCrash::initialize()` and `KF6::Crash` link.

## 7. Metadata + About
- [ ] `main.cpp`: replace `KAboutData` with `QCoreApplication` setters; drop `KLocalizedString`, `#include <KAboutData>`.
- [ ] Add `licenseNotice` property on `Controller` returning the short PolyForm text.
- [ ] Add `src/qml/pages/AboutPage.qml` (FormCard page: name, version, license notice, "Open links in the right browser" tagline) and wire it into the Settings sidebar.
- [ ] Any `i18n()` call sites → `tr()`.

## 8. Build + link list cleanup
- [ ] `src/CMakeLists.txt`: remove all KF6::* links (except documented WindowSystem exception if spike failed); ensure Qt6::Gui gets Widgets/DBus as needed.
- [ ] `CMakeLists.txt`: shrink `find_package(KF6 ...)` to the exception set (or remove entirely).
- [ ] Verify `ldd build/bin/lane | grep -ci kf6` is 0 (or 1 for the documented exception).

## 9. Docs + spec housekeeping
- [ ] README/AGENTS: update dependency lists (Qt6 + LayerShellQt; drop KF6 build deps).
- [ ] `docs/plan-windows-port.md`: mark milestone 1 done; note remaining exception if any.
- [ ] `AGENTS.md` sample JSON: add `hiddenTargetIds` (pre-existing gap).
- [ ] `openspec validate bare-qt6-port --strict` clean.

## 10. Verification sweep
- [ ] Full clean build + `ctest` (14/14 expected).
- [ ] `cmake --install` + daemon restart; `gtk-launch` routes a URL; `--settings` opens; duplicate launch forwards; notification with action renders; tray click opens settings.
- [ ] macOS rebuild sanity: lane-core + tests still green on andys-macbook-pro (`ssh andys-macbook-pro 'cd ~/src/lane && git pull && cmake --build build && ctest --test-dir build'`).
