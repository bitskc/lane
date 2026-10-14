# Tasks: bare-qt6-port

Ordered; each task leaves the Linux build green. Regression tests cover the routing core, IPC and QML controls; the runtime/API sweep covers native activation, notifications, tray, overlays and fatal termination.

## 1. Platform backend
- [x] Create `src/app/platform/platform.h` with the `Platform` interface exactly as designed: `notify(summary, body, iconName, actionId, actionLabel, action)`, `autostartSet/Enabled`, `requestActivationToken(QWindow*, done, timeoutMs)` — async token producer for the launched child, NOT raiseWindow (Lane's self-raise stays plain `requestActivate()`).
- [x] Create `src/app/platform/platform_linux.cpp` implementing the interface by calling through to the existing code paths (thin forwarding; no behavior change).
- [x] Register `platform_linux.cpp` in `src/CMakeLists.txt` under `LANE_KDE_DESKTOP`.

## 2. Single instance + activation handoff
- [x] `SingleInstance`: probe before bind; serialize absent/stale socket removal and listen with `QLockFile`; unlock before delivery waits. Forward newline-terminated `{args, workingDir, activationToken}` JSON and exit zero only after exact `ok` acknowledgment. Ambiguous connection/delivery failures exit nonzero without replaying a URL or replacing a live socket.
- [x] `src/app/DBusActivation.cpp/h`: `QDBusAbstractAdaptor` registered on `/app/lane/Lane` AND `/MainApplication`, interfaces `org.freedesktop.Application` (Activate/Open/ActivateAction) + `org.kde.KDBusService.CommandLine`. Handlers read `platform_data["activation-token"]`/`["desktop-startup-id"]` → set `XDG_ACTIVATION_TOKEN` for the dispatch duration, unset after.
- [x] `main.cpp`: replace `KDBusService` — SingleInstance claim BEFORE `Controller` construction (preserves the audit ordering fix); bus-name claim after; failure fatal in daemon mode. Remove `KF6::DBusAddons` link.
- [x] Verify both D-Bus object paths, real `gtk-launch` cold activation, duplicate-process URL/token handoff, and real stale Unix-socket recovery after SIGKILL. Native/API probes and `test_singleinstance` passed on 2026-10-13.

## 3. Tray → QSystemTrayIcon
- [x] Replace `KStatusNotifierItem` with `QSystemTrayIcon`: icon `QIcon::fromTheme("app.lane.Lane")`, tooltip "Lane"/"Link router", `Activated` → openSettings, context menu exactly {Settings → openSettings, Rediscover browsers → rediscover} — NO Quit (setStandardActionsEnabled(false) parity).
- [x] Drop `KF6::StatusNotifierItem` link. Accept Passive-status loss (icon always visible).

## 4. Notifications → platform()->notify()
- [x] Linux `Platform::notify`: `org.freedesktop.Notifications.Notify` via `QDBusMessage` — app_name "Lane", icon from call site, actions `["restore","Restore Lane"]`, hints incl. `desktop-entry:"app.lane.Lane"` (replaces `setComponentName`), timeout ~10s. `ActionInvoked`/`NotificationClosed` → dispatch stored lambda by id. Bus absent → no-op + qWarning (showMessage uses the same service; the Overview default-browser button is the documented restore fallback).
- [x] Migrate the 5 `KNotification` sites (takeover+restore, save-failed, launch-failed, opened ×2) keeping each site's `iconName`.
- [x] Drop `KF6::Notifications` link; delete `data/app.lane.Lane.notifyrc` + its install rule.

## 5. Crash log
- [x] `CrashLog`: 200 published lock-free lines, 2048 bytes per line; `sigaction` and a 64 KiB alternate stack on the main thread; POSIX-only fatal I/O; default disposition restored. `AppDataLocation/crash-<epoch>-<pid>.log`, mode 0600. Install after application metadata.
- [x] Remove `KCrash::initialize()`, `#include <KCrash>` in main.cpp AND the stray one in Controller.cpp:20. Drop `KF6::Crash` link.

## 6. Metadata
- [x] `main.cpp`: delete `KAboutData` block + `KLocalizedString`; existing `QCoreApplication` setters already carry name/version/org/desktop-file. `i18n()` → `tr()` sweep.
- [x] No AboutPage — Overview's Information card already shows version + PolyForm notice (KAboutData license text was never rendered).

## 7. Activation token
- [x] `find_package(KF6WindowSystem QUIET)`; present → link `KF6::WindowSystem` + `HAVE_KWAYLANDEXTRAS`, `requestActivationToken` wraps `KWaylandExtras::xdgActivationToken` + existing 300ms fallback; absent → `done("")` immediately. Record the documented exception in design.md/spec.
- [x] Keep `Plasma::Activities`/`HAVE_PLASMA_ACTIVITIES` as-is (Linux-only, not KF6).

## 8. Build + link cleanup
- [x] `src/CMakeLists.txt`: drop `KF6::I18n/CoreAddons/ConfigCore/DBusAddons/Notifications/IconThemes/ColorScheme/StatusNotifierItem/Crash` (ConfigCore/IconThemes/ColorScheme have zero call sites; WindowSystem is kept optional for KWaylandExtras); add SingleInstance/DBusActivation/CrashLog/platform_linux sources.
- [x] `CMakeLists.txt`: replace KF6 aggregate find with `find_package(KF6WindowSystem QUIET)` only.
- [x] CI covers the normal build plus a second build with `KF6WindowSystem` and `PlasmaActivities` discovery disabled. The equivalent clean local minimal build passed.
- [x] Dependency/docs cleanup removes Kirigami and unused KF6 libraries. Source builds keep WindowSystem/Activities optional; the existing package recipe chooses both integrations and declares their runtime dependencies.
- [x] ELF links at most one KF6 library (optional WindowSystem); the minimal build links none. KDE's platformtheme may load other Framework libraries at runtime. Settings/picker rendering loaded zero Kirigami mappings.

## 9. Docs + verification sweep
- [x] `docs/plan-windows-port.md`: mark milestone 1 done w/ exception list; `openspec validate bare-qt6-port --strict` clean.
- [x] Runtime/API acceptance sweep, 2026-10-13. UI and keyboard probes used private nested KWin (`--virtual --no-lockscreen --no-global-shortcuts --no-kactivities`), not the user's locked compositor.
  - [x] `gtk-launch app.lane.Lane <url>` cold activation delivered the URL and incoming KWin activation token to a harmless real executable.
  - [x] All four Settings pages rendered; slow drag, rename, rule editing, custom-app creation and preference persistence worked.
  - [x] A real duplicate invocation delivered exactly once and left the primary socket inode unchanged.
  - [x] Rediscover removed a deleted Firefox profile and its private counterpart from discovery and the visible Settings list.
  - [x] Native Qt/KDE StatusNotifier `Activate` opened Settings; the real menu contains Settings/Rediscover and no Quit. This is API activation, not a physical panel click.
  - [x] A real private-file takeover emitted `Notify` with Restore Lane and the desktop-entry hint. `ActionInvoked` invoked restoration through real xdg utilities. Only private HTTP/HTTPS defaults changed; HTML, mail, PDF, about and unknown defaults stayed intact. Protocol test peers were used; Plasma's popup itself was not inspected.
  - [x] SIGKILL left a real stale Unix socket; the replacement daemon reclaimed it and routed the next URL.
  - [x] Installed `lane.service` reached active/running with Type=dbus; the service MainPID matched the session bus owner PID.
  - [x] Raised/hardware SIGSEGV, std::terminate and stack overflow produced private crash logs; interrupting the logger while its mutex was held no longer hung.
- [x] Linux `cmake --build build` and `ctest --test-dir build --output-on-failure`: 16/16, including IPC and the QML behavioral runner.
- [x] macOS: `ctest --test-dir build` 13/13 (no appstreamtest) on `andys-macbook-pro`. CONFIRMED 2026-10-13 by parent at HEAD 77d0eb7: cmake configure/build succeeded (Qt 6.11.2), 13/13 ctest pass.
- [x] macOS rebuild sanity: `ssh andys-macbook-pro 'cd ~/src/lane && git pull && cmake --build build && ctest --test-dir build'`. CONFIRMED 2026-10-13 by parent at HEAD 77d0eb7 (same run as above).
