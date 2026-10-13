# Tasks: bare-qt6-port

Ordered; each task leaves the build green on Linux. Verification bars call out which gate actually proves each task (ctest covers none of the changed code — it is a regression gate only; the task-9 manual sweep is the real acceptance).

## 1. Platform seam scaffolding
- [x] Create `src/app/platform/platform.h` with the `Platform` interface exactly as designed: `notify(summary, body, iconName, actionId, actionLabel, action)`, `autostartSet/Enabled`, `requestActivationToken(QWindow*, done, timeoutMs)` — async token producer for the launched child, NOT raiseWindow (Lane's self-raise stays plain `requestActivate()`).
- [x] Create `src/app/platform/platform_linux.cpp` implementing the interface by calling through to the existing code paths (thin forwarding; no behavior change).
- [x] Register `platform_linux.cpp` in `src/CMakeLists.txt` under `LANE_KDE_DESKTOP`.

## 2. Single instance + activation handoff
- [x] `src/app/SingleInstance.cpp/h`: `QLocalServer` on `$XDG_RUNTIME_DIR/lane-instance.sock` (fallback `QStandardPaths::RuntimeLocation`), `UserAccessOption`. Startup order: listen → on AddressInUse try connect; connected → forward `{args, workingDir, activationToken}` JSON + exit 0; connect fails → `removeServer` + retry listen once → still fails → exit nonzero.
- [x] `src/app/DBusActivation.cpp/h`: `QDBusAbstractAdaptor` registered on `/app/lane/Lane` AND `/MainApplication`, interfaces `org.freedesktop.Application` (Activate/Open/ActivateAction) + `org.kde.KDBusService.CommandLine`. Handlers read `platform_data["activation-token"]`/`["desktop-startup-id"]` → set `XDG_ACTIVATION_TOKEN` for the dispatch duration, unset after.
- [x] `main.cpp`: replace `KDBusService` — SingleInstance claim BEFORE `Controller` construction (preserves the audit ordering fix); bus-name claim after; failure fatal in daemon mode. Remove `KF6::DBusAddons` link.
- [ ] Verify: `qdbus6 app.lane.Lane /app/lane/Lane` lists `Open`; `gtk-launch app.lane.Lane <url>` routes; duplicate `lane <url>` forwards with token; `rm`-simulated stale socket recovers.

## 3. Tray → QSystemTrayIcon
- [x] Replace `KStatusNotifierItem` with `QSystemTrayIcon`: icon `QIcon::fromTheme("app.lane.Lane")`, tooltip "Lane"/"Link router", `Activated` → openSettings, context menu exactly {Settings → openSettings, Rediscover browsers → rediscover} — NO Quit (setStandardActionsEnabled(false) parity).
- [x] Drop `KF6::StatusNotifierItem` link. Accept Passive-status loss (icon always visible).

## 4. Notifications → platform()->notify()
- [x] Linux `Platform::notify`: `org.freedesktop.Notifications.Notify` via `QDBusMessage` — app_name "Lane", icon from call site, actions `["restore","Restore Lane"]`, hints incl. `desktop-entry:"app.lane.Lane"` (replaces `setComponentName`), timeout ~10s. `ActionInvoked`/`NotificationClosed` → dispatch stored lambda by id. Bus absent → no-op + qWarning (showMessage uses the same service; the Overview default-browser button is the documented restore fallback).
- [x] Migrate the 5 `KNotification` sites (takeover+restore, save-failed, launch-failed, opened ×2) keeping each site's `iconName`.
- [x] Drop `KF6::Notifications` link; delete `data/app.lane.Lane.notifyrc` + its install rule.

## 5. Crash log
- [x] `src/app/CrashLog.cpp/h`: `qInstallMessageHandler` ring buffer (~200 entries) + `std::set_terminate` + `signal()` handlers for SIGSEGV/SIGABRT/SIGBUS/SIGILL/SIGFPE (best-effort write, restore default, re-raise). Output `QStandardPaths::AppDataLocation/crash-<epoch>.log`.
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
- [x] CI: shrink `.github/workflows/ci.yml` pacman deps to qt6 + layershellqt (+ kwindowsystem) — CI becomes the KF6-free proof.
- [x] `packaging/PKGBUILD`, `CONTRIBUTING.md`, `CLAUDE.md`, `README.md`: update dependency lists (drop all KF6 except optional kwindowsystem; drop kirigami only in qqc2-ui-port).
- [x] Gates: `ldd build/bin/lane | grep -cE 'libKF6'` ≤ 1 (`libKF6WindowSystem` allowed); runtime `grep -c libKF6 /proc/$(pidof lane)/maps` same bound (org.kde.desktop style can still load KF6 until qqc2-ui-port removes the setStyle call — document this).

## 9. Docs + verification sweep
- [x] `docs/plan-windows-port.md`: mark milestone 1 done w/ exception list; `openspec validate bare-qt6-port --strict` clean.
- [x] Manual sweep (the real gate — ctest does not cover this code): `gtk-launch` routes a URL; `--settings` opens; duplicate launch forwards; `lane --rediscover` against running daemon; tray click → settings + menu has Settings/Rediscover; takeover notification shows Restore action and it re-registers; stale-socket recovery; `systemctl --user status lane` reaches active via Type=dbus; kill daemon → crash log file written.
- [x] `ctest` regression gate: 14/14 on Linux (incl. appstreamtest), 13/13 on macOS (no appstreamtest).
- [x] macOS rebuild sanity: `ssh andys-macbook-pro 'cd ~/src/lane && git pull && cmake --build build && ctest --test-dir build'`.
