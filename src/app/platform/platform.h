#pragma once

#include <QString>

#include <functional>

class QWindow;

namespace Lane
{

// Per-OS platform seam: the three small KDE-flavored things the app layer
// needs that have no pure-Qt equivalent (desktop notifications, autostart
// registration, and an xdg-activation token for handing focus to a spawned
// child). Everything else in the app stays plain Qt/Kirigami and never
// touches this interface.
struct Platform {
    virtual ~Platform() = default;

    // Shows a desktop notification. `actionId`/`actionLabel` add a single
    // action button; `action` runs when it is clicked. Leave all three
    // empty for a plain informational notification.
    virtual void notify(const QString &summary, const QString &body,
                        const QString &iconName,
                        const QString &actionId = {}, const QString &actionLabel = {},
                        std::function<void()> action = {}) = 0;

    virtual void autostartSet(bool on) = 0;
    virtual bool autostartEnabled() const = 0;

    // Produces an xdg-activation token asynchronously for export into a
    // spawned child's environment. Not for raising Lane's own windows
    // (that stays plain QWindow::requestActivate()). `done` is always
    // called exactly once, with an empty string if no token could be
    // produced within `timeoutMs`.
    virtual void requestActivationToken(QWindow *source,
                                        std::function<void(QString)> done,
                                        int timeoutMs = 300) = 0;
};

// Returns the per-OS Platform singleton (Linux implementation for now).
Platform *platform();

} // namespace Lane
