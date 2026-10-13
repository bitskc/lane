# Design: Bare Qt6 Port

## Context

`src/core/` is pure Qt6 and already builds + tests on macOS (`LANE_KDE_DESKTOP` gate). The app layer (`src/app/`) carries the KDE surface. Full inventory (verified by grep + qdbus6 introspection of the live daemon, post-review corrections applied):

- `main.cpp`: `KAboutData` block incl. `setLicenseText` (the text exists but no UI ever displays it — OverviewPage already shows the PolyForm notice), `KCrash::initialize()`, `KLocalizedString::setApplicationDomain`, `KDBusService(Unique)` with `activateRequested`/`openRequested` handoffs, `QQuickStyle::setStyle("org.kde.desktop")` (line 69 — see qqc2-ui-port for its removal). No `QDBusConnection` include today.
- `Controller.cpp/h`: `KStatusNotifierItem` tray with **title, tooltip title+subtitle, Passive status, `setStandardActionsEnabled(false)` (Quit deliberately suppressed), and a context menu of "Settings" + "Rediscover browsers"** (Controller.cpp:56-70); 5 `KNotification` sites each with `setIconName` + `setComponentName("app.lane.Lane")` (desktop-entry hint drives Plasma grouping/icon/DND); `KWaylandExtras::xdgActivationToken` (sole KWindowSystem use, Controller.cpp:1064 — token is produced for Lane's own surface then exported to the **launched child's** environment via `launchTarget`); `KActivities::Consumer`+`Info` under `HAVE_PLASMA_ACTIVITIES` (Plasma::Activities, not KF6, stays Linux-only); a stray unused `#include <KCrash>` (Controller.cpp:20).
- Live D-Bus contract (qdbus6 on running daemon): name `app.lane.Lane` owns object path `/app/lane/Lane` exposing `org.freedesktop.Application` (`Activate`/`Open`/`ActivateAction` with `platform_data`) **and** `org.kde.KDBusService.CommandLine(as, s, a{sv})`, plus `/MainApplication` with the same interfaces. `XDG_ACTIVATION_TOKEN` is read out of Lane's own env during dispatch (Controller.cpp:493-504) because KDBusService sets it from `platform_data["activation-token"]`/`desktop-startup-id`.
- systemd unit is `Type=dbus` `BusName=app.lane.Lane` `Restart=on-failure` — bus-name acquisition is the readiness signal; failure must stay fatal or the unit restart-loops.
- Dead KF6 links with zero call sites: `KF6::ConfigCore`, `KF6::IconThemes`, `KF6::ColorScheme` (KF6::WindowSystem is not dead — it hosts KWaylandExtras for the activation token).

## Goals / Non-Goals

**Goals**: on Linux, `lane` links **no KF6 module except optionally `KF6::WindowSystem`** (documented exception for the xdg-activation token — see Decision 7); identical user-visible behavior; `DBusActivatable`, systemd Type=dbus readiness, notification actions, and the activation-token relay all preserved; platform seam shaped for future Windows/macOS backends.

**Non-Goals**: no feature additions (no About page — Overview's Information card already covers metadata + license; no tray Quit — it is deliberately disabled today); QML stays Kirigami (separate change `qqc2-ui-port`); no Windows/macOS backends; LayerShellQt stays (verified: links only Qt6 + libwayland, no KF6); ECM stays on Linux; PlasmaActivities stays optional-Linux.

## Decisions

### 1. Platform seam: async token producer, not raiseWindow

`src/app/platform/platform.h`:

```cpp
struct Platform {
    virtual ~Platform() = default;
    virtual void notify(const QString &summary, const QString &body,
                        const QString &iconName,
                        const QString &actionId = {},
                        const QString &actionLabel = {},
                        std::function<void()> action = {}) = 0;
    virtual void autostartSet(bool on) = 0;
    virtual bool autostartEnabled() const = 0;
    // Produces an xdg-activation token asynchronously for export into a
    // spawned child's environment. Not for raising Lane's own windows
    // (that stays plain QWindow::requestActivate()).
    virtual void requestActivationToken(QWindow *source,
                                        std::function<void(QString)> done,
                                        int timeoutMs = 300) = 0;
};
Platform *platform();
```

Lane's self-raise sites (Controller.cpp:684, 999, 1194) are already plain `requestActivate()` — they stay as-is. The seam exists for: notify, autostart, activation-token. Discovery/default-browser/sourceInfo seams land with their respective port changes, not preemptively.

### 2. Single instance: two transports, ordered claim, token-carrying payloads

**Uniqueness & failure contract** (resolves the two-tokens split-brain):
1. `QLocalServer` on a filesystem socket under `$XDG_RUNTIME_DIR/lane-instance.sock` (falls back to `QStandardPaths::RuntimeLocation`), `setSocketOptions(QLocalServer::UserAccessOption)` — no `/tmp`, no other-UID access.
2. If `listen()` fails with `AddressInUseError`: attempt `QLocalSocket` connect. Connect succeeds → forward payload `{args, workingDir, activationToken}` (JSON; `activationToken` = `qEnvironmentVariable("XDG_ACTIVATION_TOKEN")` of the client) and exit 0. Connect fails → stale socket from a crashed daemon: `QLocalServer::removeServer`, retry `listen()` once. Still fails → hard error, exit nonzero.
3. Then register D-Bus name `app.lane.Lane`. Registration failure in daemon/activation mode is fatal (same semantic as `KDBusService::Unique`, and it is the systemd readiness signal). CLI modes that never daemonize (`--list`, `--explain`, `--config-path`) skip all of this exactly as today.

**D-Bus adaptor**: a `QDBusAbstractAdaptor` registered on `/app/lane/Lane` **and** `/MainApplication` (parity with KDBusService's live export — verified via qdbus6), exposing `org.freedesktop.Application` (`Activate`, `Open`, `ActivateAction`) and `org.kde.KDBusService.CommandLine`. Each handler reads `platform_data["activation-token"]`/`["desktop-startup-id"]` and sets `XDG_ACTIVATION_TOKEN` in the process env for the duration of the `handleArgs`/`openUrl` dispatch, unsetting after — mirroring KDBusService's documented behavior (Controller.cpp:493-504 depends on it).

### 3. Tray: QSystemTrayIcon, exact-parity menu

`QSystemTrayIcon` with `QIcon::fromTheme("app.lane.Lane")`, `Activated` → `openSettings()`, `setContextMenu(QMenu{ "Settings" → openSettings, "Rediscover browsers" → rediscover })`. No Quit (deliberately disabled today via `setStandardActionsEnabled(false)`). Tooltip "Lane / Link router" parity. Loss acknowledged in spec: `QSystemTrayIcon` has no Passive status — the icon is permanently visible instead of Passive-eligible; accepted (SNI-Passive only affects tray auto-hide grouping).

### 4. Notifications: org.freedesktop.Notifications over QDBus

`Platform::notify` on Linux: `Notify` call with app_name `"Lane"`, icon (from call-site `iconName` — all 5 sites set it), `actions` array `["restore", "Restore Lane"]`, hints including `desktop-entry: "app.lane.Lane"` (replaces `setComponentName`; preserves Plasma grouping/DND), expire timeout ~10s. `ActionInvoked`/`NotificationClosed` signals dispatch the stored lambda by notification id. No session bus → no-op with a qWarning: `QSystemTrayIcon::showMessage` routes through the *same* notification service so it is not a real fallback; the watchdog's restore path is the existing Overview-page default-browser button (already in Settings, no new UI needed — spec states this).

### 5. Crash log: message handler + terminate + fatal signals

`qInstallMessageHandler` ring buffer (~200 lines) mirrored to stderr; `std::set_terminate` flushes to `QStandardPaths::AppDataLocation/crash-<epoch>.log`. Also `signal()` handlers for SIGSEGV/SIGABRT/SIGBUS/SIGILL/SIGFPE: write best-effort, restore default, re-raise (no core suppression). Log content inherits whatever was logged — same as stderr today.

### 6. Metadata: QCoreApplication only

Delete the `KAboutData` block + `KLocalizedString`; `app.setApplicationName/Version/OrganizationName/Domain/DesktopFileName` are already set and stay. No About page (Overview Information card already shows version + PolyForm notice; KAboutData's license text was dead — nobody renders it). `i18n()`→`tr()` sweep (only `KLocalizedString` remains in main.cpp).

### 7. Activation token: optional KF6::WindowSystem, raw-protocol fallback out of scope

The token exists only on Wayland and only preserves the focus-handoff nicety (300ms fallback already covers its absence). `find_package(KF6WindowSystem)` — **present → link + `HAVE_KWAYLANDEXTRAS`, identical behavior; absent → `done("")` immediately** (launched browser opens unfocused, same as every non-KDE platform will get permanently). Hand-rolling xdg-activation-v1 via `QNativeInterface::QWaylandApplication` + wayland-scanner is deferred: it needs a build-time wayland-scanner + wayland-client dependency chain for a nicety; record as a follow-up. Spec wording: "no required KF6 dev package; WindowSystem MAY be linked when present for the activation-token feature."

### 8. Build changes

`src/CMakeLists.txt`: drop `KF6::I18n/CoreAddons/ConfigCore/DBusAddons/Notifications/IconThemes/ColorScheme/StatusNotifierItem/Crash`; conditionally `KF6::WindowSystem` + `HAVE_KWAYLANDEXTRAS`; add `platform/platform_linux.cpp`, `app/SingleInstance.cpp`, `app/CrashLog.cpp` under `LANE_KDE_DESKTOP`; drop `data/app.lane.Lane.notifyrc` install + file. `CMakeLists.txt`: `find_package(KF6)` replaced by per-module finds (WindowSystem QUIET). Delete `#include <KCrash>` in Controller.cpp. CI: shrink pacman list to qt6 + layershellqt (+ kwindowsystem) — the CI build itself becomes the KF6-free check. Docs: PKGBUILD deps, CONTRIBUTING, CLAUDE.md, README build section all updated.

## Risks / Trade-offs

- `DBusActivatable` regression → task pins both object paths and both interfaces; verification includes `qdbus6` introspection AND `gtk-launch` + a real click end-to-end (this exact path broke once before).
- Wayland token without WindowSystem → degraded to no-token; documented in spec.
- Plasma notification action rendering of hand-built Notify → UNVERIFIABLE from repo; live-test on Plasma in verification sweep.
- `QSystemTrayIcon` Passive-status loss → documented spec acceptance.
- QLocalSocket payload injection by same-UID processes → same trust boundary as session D-Bus today; accepted.

## Migration Plan

Single branch; ordered commits per tasks.md; build + tests green after each task. Install to `~/.local`, restart daemon (`systemctl --user restart lane` — note: `pkill -x lane` triggers Restart=on-failure anyway), verify `gtk-launch` + real click. **Rollback**: git revert + rebuild + `cmake --install` + restart daemon; the D-Bus service file and autostart entry keep pointing at the installed binary — they need no edits. Plasma notification-settings residue under app.lane.Lane is harmless.

## Open Questions

- Whether Plasma 6.7 renders a hand-built `Notify` actions array identically to KNotification (live-test in verification; fallback is the Overview restore path).
- Whether launchers send `activation-token` vs `desktop-startup-id` in practice (adaptor handles both).
