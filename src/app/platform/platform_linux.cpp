#include "platform.h"
#include "linux_handler_state.h"
#include "app/Autostart.h"
#include "app/DBusActivation.h"

#include <LayerShellQt/Window>
#include <QClipboard>
#include <QGuiApplication>
#include <QProcess>

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
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
#include <utility>

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
        auto pending = m_pending.take(id);
        if (pending.first == actionKey && pending.second) {
            pending.second();
        }
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

    AutostartStatus autostartStatus() override
    {
        AutostartStatus s;
        s.enabled = Lane::autostartEnabled();
        return s;
    }

    bool watchesDefaultHandlerByFile() const override { return true; }

    DefaultHandlerState defaultHandlerState() override
    {
        const auto query = [](const char *scheme) {
            QProcess process;
            process.start(QStringLiteral("xdg-mime"),
                          {QStringLiteral("query"), QStringLiteral("default"),
                           QStringLiteral("x-scheme-handler/") + QLatin1String(scheme)});
            const bool ok = process.waitForFinished(1500) && process.exitCode() == 0;
            return std::pair<bool, QString>{ok, QString::fromUtf8(process.readAllStandardOutput()).trimmed()};
        };
        const auto http = query("http");
        const auto https = query("https");
        return linuxDefaultHandlerState(http.first, http.second, https.first, https.second);
    }

    void makeDefaultHandler(std::function<void(bool ok)> done) override
    {
        QProcess::execute(QStringLiteral("xdg-mime"),
                          {QStringLiteral("default"), QStringLiteral("app.lane.Lane.desktop"), QStringLiteral("x-scheme-handler/http")});
        QProcess::execute(QStringLiteral("xdg-mime"),
                          {QStringLiteral("default"), QStringLiteral("app.lane.Lane.desktop"), QStringLiteral("x-scheme-handler/https")});
        const DefaultHandlerState state = defaultHandlerState();
        done(state.isDefault);
    }

    void configureOverlayWindow(QWindow *window, const QString &scope) override
    {
        auto *ls = LayerShellQt::Window::get(window);
        if (!ls) { // null on X11 / non-wlroots — window behaves as normal
            qWarning() << "Lane: layer-shell unavailable; falling back to normal window";
            return;
        }
        ls->setLayer(LayerShellQt::Window::LayerOverlay);
        LayerShellQt::Window::Anchors anchors;
        anchors.setFlag(LayerShellQt::Window::AnchorTop);
        anchors.setFlag(LayerShellQt::Window::AnchorBottom);
        anchors.setFlag(LayerShellQt::Window::AnchorLeft);
        anchors.setFlag(LayerShellQt::Window::AnchorRight);
        ls->setAnchors(anchors);
        ls->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityExclusive);
        ls->setExclusiveZone(-1);
        ls->setScope(scope);
        ls->setWantsToBeOnActiveScreen(true);
        ls->setActivateOnShow(true);
        auto *screen = window->screen() ? window->screen() : QGuiApplication::primaryScreen();
        if (screen) {
            window->setGeometry(screen->geometry());
        }
    }

    bool registerActivation(std::function<void(const QStringList &args)> handleArgs,
                            std::function<void(const QString &url)> openUrl) override
    {
        return Lane::registerActivation(std::move(handleArgs), std::move(openUrl));
    }

    bool prefersReducedMotion() const override
    {
        // Linux has no single "reduce motion" setting that is practical
        // to poll across desktops; the animations it would gate are the
        // macOS-cheap ones, so false is the honest answer here.
        return false;
    }

    QString clipboardText() const override
    {
        QString text;
        if (auto *clip = QGuiApplication::clipboard()) {
            text = clip->text().trimmed();
        }
        // Plasma Wayland: unfocused clients cannot read the selection.
        // Klipper keeps a copy and exposes it over D-Bus.
        if (text.isEmpty()) {
            QDBusInterface klipper(QStringLiteral("org.kde.klipper"),
                                   QStringLiteral("/klipper"),
                                   QStringLiteral("org.kde.klipper.klipper"),
                                   QDBusConnection::sessionBus());
            if (klipper.isValid()) {
                const QDBusReply<QString> reply = klipper.call(QStringLiteral("getClipboardContents"));
                if (reply.isValid()) {
                    text = reply.value().trimmed();
                }
            }
        }
        return text;
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
