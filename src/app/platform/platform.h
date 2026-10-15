#pragma once

#include <QString>
#include <QStringList>

#include <functional>

class QWindow;

namespace Lane
{

// Per-OS platform seam: the desktop bits the app layer needs that have no
// pure-Qt equivalent — desktop notifications, autostart/login-item
// registration, the default-handler query and setter, an xdg-activation token
// for spawned children, and an overlay window. Everything else in the app
// stays plain Qt/Kirigami and never touches this interface.
struct Platform {
    virtual ~Platform() = default;

    // Shows a desktop notification. `actionId`/`actionLabel` add a single
    // action button; `action` runs when it is clicked. Leave all three
    // empty for a plain informational notification.
    virtual void notify(const QString &summary, const QString &body,
                        const QString &iconName,
                        const QString &actionId = {}, const QString &actionLabel = {},
                        std::function<void()> action = {}) = 0;

    // macOS uses SMAppService for login items; Linux uses an XDG autostart
    // desktop file. macOS apps using SMAppService must be code signed; a
    // build with no signing identity surfaces that here as an error.
    enum class AutostartError { None, NotRegistered, RequiresApproval, InvalidSignature };
    struct AutostartStatus {
        bool enabled = false;
        AutostartError error = AutostartError::None;
    };
    virtual void autostartSet(bool on) = 0;
    virtual bool autostartEnabled() const = 0;
    virtual AutostartStatus autostartStatus() = 0;

    struct DefaultHandlerState {
        // true when Lane is the handler for BOTH http and https.
        bool isDefault = false;
        // A human-readable handler name for the Settings UI, empty when the
        // host cannot report one.
        QString currentHandler;
        enum class Status { Registered, Declined, Partial, OtherCopy, Unknown };
        Status status = Status::Unknown;
    };
    // Returns whether this OS watches a filesystem file for default-handler
    // changes. Linux watches mimeapps.list; macOS polls LaunchServices.
    virtual bool watchesDefaultHandlerByFile() const = 0;
    virtual DefaultHandlerState defaultHandlerState() = 0;
    // `done` is invoked on the platform thread; called at most once.
    virtual void makeDefaultHandler(std::function<void(bool ok)> done) = 0;

    // macOS: no compositor grab. Configures a borderless, top-most,
    // key-capable overlay window across Spaces (collection behavior).
    virtual void configureOverlayWindow(QWindow *window, const QString &scope = QStringLiteral("lane-picker")) = 0;

    // Registers the activation contract that lets a launcher or a second
    // process reach this running daemon. On Linux this is the D-Bus
    // org.freedesktop.Application / org.kde.KDBusService export; on macOS,
    // LaunchServices delivers QFileOpenEvent and no registration exists.
    // Returning false means registration is impossible and fatal on Linux.
    virtual bool registerActivation(std::function<void(const QStringList &args)> handleArgs,
                                    std::function<void(const QString &url)> openUrl) = 0;

    // Produces an xdg-activation token asynchronously for export into a
    // spawned child's environment. Not for raising Lane's own windows
    // (that stays plain QWindow::requestActivate()). `done` is always
    // called exactly once, with an empty string if no token could be
    // produced within `timeoutMs`.
    virtual void requestActivationToken(QWindow *source,
                                        std::function<void(QString)> done,
                                        int timeoutMs = 300) = 0;

    // Whether the OS is asking apps to reduce motion ("Reduce Motion" in
    // macOS Accessibility). Used to gate the Hold ring and dim-fade
    // animations. Linux returns false (no system-wide equivalent to poll).
    virtual bool prefersReducedMotion() const = 0;

    // True after a notification post was refused (macOS "denied" in
    // System Settings). Drives the denied banner in Overview. Default
    // false: Linux toast backends cannot distinguish denied from quiet.
    virtual bool notificationsDenied() const { return false; }
};

// Returns the per-OS Platform singleton.
Platform *platform();

} // namespace Lane
