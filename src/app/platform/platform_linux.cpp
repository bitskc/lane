#include "platform.h"

#include "app/Autostart.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QHash>
#include <QObject>
#include <QPair>
#include <QTimer>
#include <QVariantMap>
#include <QWindow>

#include <memory>

#ifdef HAVE_KWAYLANDEXTRAS
#include <KWaylandExtras>
#endif

namespace Lane
{
namespace
{

// Dispatches org.freedesktop.Notifications ActionInvoked/NotificationClosed
// signals back to the lambda a notify() call supplied, keyed by the
// notification id the Notify call returned. Needs Q_OBJECT for
// QDBusConnection::connect's slot matching, so it is moc'd out of this
// .cpp file directly (see the #include at the bottom).
class NotificationDispatcher : public QObject
{
    Q_OBJECT
public:
    static NotificationDispatcher &instance()
    {
        static NotificationDispatcher dispatcher;
        return dispatcher;
    }

    void remember(uint id, const QString &actionId, std::function<void()> action)
    {
        m_pending.insert(id, qMakePair(actionId, std::move(action)));
    }

private Q_SLOTS:
    void onActionInvoked(uint id, const QString &actionKey)
    {
        const auto it = m_pending.find(id);
        if (it == m_pending.end()) {
            return;
        }
        if (it->first == actionKey && it->second) {
            it->second();
        }
        m_pending.erase(it);
    }

    void onNotificationClosed(uint id, uint)
    {
        m_pending.remove(id);
    }

private:
    NotificationDispatcher()
    {
        QDBusConnection::sessionBus().connect(
            QStringLiteral("org.freedesktop.Notifications"),
            QStringLiteral("/org/freedesktop/Notifications"),
            QStringLiteral("org.freedesktop.Notifications"),
            QStringLiteral("ActionInvoked"),
            this, SLOT(onActionInvoked(uint,QString)));
        QDBusConnection::sessionBus().connect(
            QStringLiteral("org.freedesktop.Notifications"),
            QStringLiteral("/org/freedesktop/Notifications"),
            QStringLiteral("org.freedesktop.Notifications"),
            QStringLiteral("NotificationClosed"),
            this, SLOT(onNotificationClosed(uint,uint)));
    }

    QHash<uint, QPair<QString, std::function<void()>>> m_pending;
};

// Needs to be a QObject too: KWaylandExtras::xdgActivationToken()'s
// QFuture::then(context, ...) continuation and the fallback QTimer both
// want a context object that outlives the request, so the singleton
// itself (never destroyed before the app exits) serves as that context.
class LinuxPlatform final : public QObject, public Platform
{
    Q_OBJECT
public:
    void notify(const QString &summary, const QString &body, const QString &iconName,
                const QString &actionId, const QString &actionLabel,
                std::function<void()> action) override
    {
        QDBusConnection bus = QDBusConnection::sessionBus();
        if (!bus.isConnected()) {
            qWarning("Lane: no D-Bus session bus available, dropping notification: %s", qPrintable(summary));
            return;
        }

        QDBusMessage msg = QDBusMessage::createMethodCall(
            QStringLiteral("org.freedesktop.Notifications"),
            QStringLiteral("/org/freedesktop/Notifications"),
            QStringLiteral("org.freedesktop.Notifications"),
            QStringLiteral("Notify"));

        QStringList actions;
        if (!actionId.isEmpty()) {
            actions << actionId << actionLabel;
        }

        QVariantMap hints;
        hints.insert(QStringLiteral("desktop-entry"), QStringLiteral("app.lane.Lane"));

        msg << QStringLiteral("Lane")   // app_name
            << quint32(0)                // replaces_id
            << iconName                  // app_icon
            << summary
            << body
            << actions
            << hints
            << qint32(10000);            // expire_timeout, ~10s

        QDBusPendingCall call = bus.asyncCall(msg);
        if (actionId.isEmpty() || !action) {
            // No action to wire up: fire and forget, nothing to dispatch
            // when ActionInvoked fires later.
            return;
        }
        auto *watcher = new QDBusPendingCallWatcher(call);
        QObject::connect(watcher, &QDBusPendingCallWatcher::finished, watcher,
                          [actionId, action](QDBusPendingCallWatcher *w) {
            const QDBusPendingReply<uint> reply = *w;
            if (!reply.isError()) {
                NotificationDispatcher::instance().remember(reply.value(), actionId, action);
            }
            w->deleteLater();
        });
    }

    void autostartSet(bool on) override
    {
        setAutostart(on);
    }

    bool autostartEnabled() const override
    {
        return Lane::autostartEnabled();
    }

    void requestActivationToken(QWindow *source, std::function<void(QString)> done, int timeoutMs) override
    {
#ifdef HAVE_KWAYLANDEXTRAS
        auto resolved = std::make_shared<bool>(false);
        auto finish = [done, resolved](const QString &token) {
            if (*resolved) {
                // Either the compositor already answered and the fallback
                // timer fired anyway, or vice versa; only the first call
                // reaches the caller.
                return;
            }
            *resolved = true;
            done(token);
        };
        KWaylandExtras::xdgActivationToken(source, QString()).then(this, finish);
        QTimer::singleShot(timeoutMs, this, [finish]() { finish(QString()); });
#else
        Q_UNUSED(source);
        Q_UNUSED(timeoutMs);
        done(QString());
#endif
    }
};

} // namespace

Platform *platform()
{
    static LinuxPlatform instance;
    return &instance;
}

} // namespace Lane

#include "platform_linux.moc"
