# Design: Bare Qt6 Port

## Context

`src/core/` is pure Qt6 and already builds + tests on macOS (`LANE_KDE_DESKTOP` gate). The app layer (`src/app/`) carries the entire KF6 surface. Full inventory (verified by grep at commit `7552c5f`):

- `main.cpp`: `KAboutData` (3 call sites incl. license enum), `KCrash::initialize()`, `KLocalizedString::setApplicationDomain`, `KDBusService` (unique instance + `activateRequested`/`activateActionRequested` handoff), `#include <QDBusConnection>` already present.
- `Controller.cpp/h`: `KStatusNotifierItem` tray (5 sites: ctor, Passive status, ApplicationStatus category, icon, `activateRequested` → settings), `KNotification` ×3 sites (browser-takeover with `KNotificationAction` restore, save-failed, launch-failed, opened ×2 = 5 `KNotification` ctor sites total), `KWaylandExtras::xdgActivationToken()` for launch activation, `KWindowSystem` for window activation, `KCrash` include.
- `Autostart.cpp`: XDG `.desktop` writer — pure Qt already.
- `DefaultBrowserWatcher.cpp/h`, `PickerModel.cpp`, `TargetModel.cpp`, `RuleModel.cpp`, `SourceInfo.cpp`: already KF6-free.

## Goals / Non-Goals

**Goals**: `lane` binary links zero KF6 libraries on Linux; identical user-visible behavior; platform seam exists for future Windows/macOS backends; `DBusActivatable` keeps working; notification actions preserved on Linux; every KF6 removal is verifiable via `ldd bin/lane | grep -c libKF6` → 0.

**Non-Goals**: QML stays Kirigami (separate change `qqc2-ui-port`); no Windows/macOS backend implementation yet — only the seam; no behavior/feature additions; LayerShellQt stays (Qt wrapper, not KF6); ECM stays on Linux (install dirs, version header, qmlmodule checks).

## Decisions

### 1. Platform seam: thin interface, not deep abstraction

`src/app/platform/platform.h` — a small struct of function pointers / virtual interface:

```cpp
namespace Lane {
struct Platform {
    virtual ~Platform() = default;
    virtual void notify(const QString &event, const QString &title,
                        const QString &body, const QString &actionId = {},
                        std::function<void()> action = {}) = 0;
    virtual void autostartSet(bool on) = 0;
    virtual bool autostartEnabled() const = 0;
    virtual void raiseWindow(QWindow *w) = 0;   // activation incl. Wayland token
    // discovery/default-browser seams land in later changes; declared
    // now so backends exist for them from day one
};
Platform *platform(); // returns the per-OS singleton
}
```

`platform_linux.cpp` (compiled under `LANE_KDE_DESKTOP`) implements everything with today's Linux code re-expressed on Qt APIs. `Controller` calls `platform()->...`. Alternative considered: per-feature `#ifdef` scatter — rejected, it is what makes porting painful later.

### 2. Single instance: QLocalServer + kept org.freedesktop.Application D-Bus object

Two mechanisms, both needed on Linux:

- **`QLocalServer` at key `app.lane.Lane`**: second instance connects, writes its argv payload (URL or `--settings`), running instance reads and routes. Works on every OS including Windows/macOS later.
- **D-Bus `org.freedesktop.Application` object on `app.lane.Lane`**: the installed `.desktop` declares `DBusActivatable=true`; removing KDBusService means nothing registers `Activate`/`Open`/`ActivateAction`. We implement `QDBusAbstractAdaptor`-style handlers directly on `QDBusConnection::sessionBus()` — ~120 lines, no KF6 needed. `gtk-launch app.lane.Lane` must keep working (this exact path broke once before; there is a real regression risk).

Order at startup: claim `QLocalServer` first (cheapest), then register D-Bus name. If `QLocalServer` fails to listen → forward argv over `QLocalSocket` and exit. This preserves the KDBusService ordering fix from the audit (claim uniqueness before paying discovery cost).

### 3. Tray: QSystemTrayIcon

`m_tray` becomes `QSystemTrayIcon` with `QIcon::fromTheme("app.lane.Lane")`, `Activated` → open settings, a small `QMenu` (Settings, Quit) as a parity bonus since SNI had a menu path. On Plasma, `QSystemTrayIcon` renders through StatusNotifier anyway.

### 4. Notifications: org.freedesktop.Notifications over QDBus, fallback showMessage

`platform()->notify()`: Linux implementation calls `org.freedesktop.Notifications.Notify` via `QDBusMessage` — params: app name, replaces-id 0, icon, summary, body, actions array `["restore", "Restore Lane"]`, hints, expire timeout `CloseOnTimeout` equivalent (~10s). Connect `ActionInvoked`/`NotificationClosed` signals to dispatch the action lambda keyed by notification id. This is the only notification API that preserves buttons. Fallback when D-Bus absent: `QSystemTrayIcon::showMessage` (loses the action — then surface restore as a settings banner; spec already requires the fallback path).

### 5. Crash log: message handler + terminate handler

`qInstallMessageHandler` keeps a bounded ring buffer (~200 lines) in memory and mirrors to stderr; `std::set_terminate` + the handler's `QtFatalMsg` path flush the buffer to `QStandardPaths::AppDataLocation/crash-<timestamp>.log`. No dialog, no symbols — acceptable for a resident daemon; KCrash's real value was the KDE dialog.

### 6. About/metadata: QCoreApplication + QML About card

`app.setApplicationName/Version/OrganizationName/Domain` + a `license` property on Controller returning a short PolyForm notice string. A new `AboutPage.qml` in the settings sidebar (kirigami FormCard page for now; QQC2 migration inherits it). Fixes the "Custom"-only label gap without touching KAboutData.

### 7. Activation: Wayland token behind the seam

`KWaylandExtras::xdgActivationToken`/`KWindowSystem::updateXdgActivationToken` move into `platform_linux.cpp` `raiseWindow()`. KWindowSystem is KF6 → it must go too: the xdg-activation token path is reimplemented via `QDBusMessage` to `org.kde.KWin` / or simpler: keep the Wayland branch behind `QGuiApplication::platformName()=="wayland"` using the existing `KWaylandExtras` — **no**, KWaylandExtras is also KF6-adjacent (KF6::WindowSystem). Reimplementation: xdg_activation_v1 needs a serial/token negotiation through the Wayland connection — nontrivial. **Decision**: call `QWindow::requestActivate()` and additionally run the token request through the raw Wayland interface only if trivially available via `QNativeInterface::QWaylandApplication`. Spike task in tasks.md; worst case Linux loses the focus token and launched browsers open behind the picker — regression risk flagged below. If the spike fails, keep `KF6::WindowSystem` + `KWaylandExtras` as the single permitted KF6 link with a comment, deferring full removal — a documented exception, not silent scope creep.

### 8. i18n

`KLocalizedString::setApplicationDomain` + any `i18n()` → `QObject::tr()` / `QCoreApplication::translate`. QML has zero `i18n()` uses today.

### 9. Build changes

`src/CMakeLists.txt`: drop KF6::* links entirely except the possible WindowSystem exception above; add `platform/platform_linux.cpp` under `LANE_KDE_DESKTOP`; Qt6::DBus stays Linux-only. `CMakeLists.txt`: KF6 find guarded/removed accordingly; ECM retained.

## Risks / Trade-offs

- `DBusActivatable` regression → keep `qdbus6`-based manual test in tasks; re-verify `gtk-launch` end-to-end; the prior bug (hang with no Exec fallback) is exactly this path.
- xdg-activation token loss on Wayland → spike first (task 2); if infeasible without KF6, keep `KF6::WindowSystem` as documented exception rather than degrade UX.
- Notification action loss if D-Bus notifications daemon is absent → settings-page banner fallback in spec; low probability on Plasma.
- `QSystemTrayIcon` requires a system tray on Wayland — same caveat SNI had; no behavior change.
- Wayland-only QML imports untouched — Kirigami remains until `qqc2-ui-port`.

## Migration Plan

Single branch, ordered commits per task below. Build + full `ctest` after each KF6 removal lands. Install to `~/.local` at the end; restart daemon; verify `gtk-launch` and a real click. Rollback: revert the branch — no config or data changes exist.

## Open Questions

- Whether `QSystemTrayIcon` on Plasma 6.7 Wayland exposes context menus equivalently to SNI (cosmetic; does not block).
