// macOS platform seam (stub). The macOS app uses QFileOpenEvent and
// LaunchServices instead of D-Bus; this file links Objective-C++ against
// AppKit/UserNotifications/ServiceManagement where the real macOS bodies
// will live. Every method here is intentionally an honest "unsupported on
// this build" so that `lane-core`'s picker and Settings window open, while
// the macOS-specific tasks (URL delivery, autostart, default handler,
// notifications) remain to be implemented.

#include "platform.h"

#include <QWindow>

namespace Lane
{

namespace
{

class MacPlatform final : public Platform
{
public:
    void notify(const QString &summary, const QString &body, const QString &iconName,
                const QString &actionId, const QString &actionLabel,
                std::function<void()> action) override
    {
        // TODO(macos): UNUserNotificationCenter. Until implemented, drop
        // silently (the caller already gates on toastEnabled).
        Q_UNUSED(summary);
        Q_UNUSED(body);
        Q_UNUSED(iconName);
        Q_UNUSED(actionId);
        Q_UNUSED(actionLabel);
        Q_UNUSED(action);
    }

    void autostartSet(bool on) override { Q_UNUSED(on); }
    bool autostartEnabled() const override { return false; }
    AutostartStatus autostartStatus() override
    {
        AutostartStatus s;
        s.error = AutostartError::NotRegistered;
        return s;
    }

    bool watchesDefaultHandlerByFile() const override { return false; }
    DefaultHandlerState defaultHandlerState() override
    {
        return DefaultHandlerState{}; // macOS uses LSSetDefaultHandler; not yet implemented
    }
    void makeDefaultHandler(std::function<void(bool ok)> done) override
    {
        // TODO(macos): NSWorkspace setDefaultApplication...; requires user
        // consent and (for SMAppService) a signed bundle.
        done(false);
    }

    void configureOverlayWindow(QWindow *window, const QString &scope) override
    {
        // TODO(macos): borderless top-most key-capable window across Spaces
        // (collection behavior). For now a plain top-most window so the
        // picker is visible and testable; key focus is required, so no
        // WindowDoesNotAcceptFocus.
        Q_UNUSED(scope);
        window->setFlags(Qt::Window | Qt::WindowStaysOnTopHint | Qt::FramelessWindowHint);
    }

    bool registerActivation(std::function<void(const QStringList &args)> handleArgs,
                            std::function<void(const QString &url)> openUrl) override
    {
        // LaunchServices activation needs no registration: clicks arrive as
        // QFileOpenEvent (task 4.2 installs the filter). Always "registered".
        Q_UNUSED(handleArgs);
        Q_UNUSED(openUrl);
        return true;
    }

    void requestActivationToken(QWindow *source,
                                std::function<void(QString)> done,
                                int timeoutMs) override
    {
        // macOS has no xdg-activation token; callers receive an empty token.
        Q_UNUSED(source);
        Q_UNUSED(timeoutMs);
        done(QString());
    }
};

} // namespace

Platform *platform()
{
    static MacPlatform instance;
    return &instance;
}

} // namespace Lane
