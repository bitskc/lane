#include "DBusActivation.h"

#include <QCoreApplication>
#include <QDBusAbstractAdaptor>
#include <QDBusConnection>
#include <QDBusError>
#include <QDebug>
#include <QVariant>
#include <QVariantList>
#include <QVariantMap>

namespace Lane
{
namespace
{

// Sets XDG_ACTIVATION_TOKEN from platform_data for the duration of `fn`,
// then unsets it. Mirrors KDBusService's documented behavior, which
// Controller::openUrl's own env-token read depends on.
void withActivationToken(const QVariantMap &platformData, const std::function<void()> &fn)
{
 // fn() must run synchronously inside this call: the env var is only set
 // for its duration. The token is consumed once: a call that carries a
 // token clears the variable after fn() returns, so that token cannot
 // leak into a later tokenless activation. (A call with no token leaves
 // any inherited value untouched; openUrl consumes it itself.) A URI
 // list inside one Open re-establishes the same token, so intra-call
 // reuse is unaffected.
    QString token = platformData.value(QStringLiteral("activation-token")).toString();
    if (token.isEmpty()) {
        token = platformData.value(QStringLiteral("desktop-startup-id")).toString();
    }
    if (!token.isEmpty()) {
        qputenv("XDG_ACTIVATION_TOKEN", token.toUtf8());
    }
    fn();
    if (!token.isEmpty()) {
        qunsetenv("XDG_ACTIVATION_TOKEN");
    }
}

// org.freedesktop.Application: Activate/Open/ActivateAction.
class ApplicationAdaptor : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.Application")
public:
    ApplicationAdaptor(QObject *parent,
                        std::function<void(const QStringList &)> handleArgs,
                        std::function<void(const QString &)> openUrl)
        : QDBusAbstractAdaptor(parent)
        , m_handleArgs(std::move(handleArgs))
        , m_openUrl(std::move(openUrl))
    {
    }

public Q_SLOTS:
    void Activate(const QVariantMap &platform_data)
    {
        withActivationToken(platform_data, [this]() { m_handleArgs({}); });
    }

    void Open(const QStringList &uris, const QVariantMap &platform_data)
    {
        withActivationToken(platform_data, [this, &uris]() {
            // openUrl consumes the env token on the first URI. Put it back
            // for each later URI in this same Open; one activation covers
            // the whole list.
            const QByteArray token = qgetenv("XDG_ACTIVATION_TOKEN");
            for (const QString &uri : uris) {
                if (!token.isEmpty()) {
                    qputenv("XDG_ACTIVATION_TOKEN", token);
                }
                m_openUrl(uri);
            }
        });
    }

    void ActivateAction(const QString &action_name, const QVariantList &parameter, const QVariantMap &platform_data)
    {
        Q_UNUSED(parameter);
        const QStringList args = argsForAction(action_name);
        withActivationToken(platform_data, [this, args]() { m_handleArgs(args); });
    }

private:
    std::function<void(const QStringList &)> m_handleArgs;
    std::function<void(const QString &)> m_openUrl;
};

// org.kde.KDBusService: the CommandLine method only (the rest of that
// interface, e.g. Quit, was never exposed by Lane's KDBusService(Unique)
// export either).
class CommandLineAdaptor : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.KDBusService")
public:
    CommandLineAdaptor(QObject *parent, std::function<void(const QStringList &)> handleArgs)
        : QDBusAbstractAdaptor(parent)
        , m_handleArgs(std::move(handleArgs))
    {
    }

public Q_SLOTS:
    void CommandLine(const QStringList &arguments, const QString &workingDirectory, const QVariantMap &platform_data)
    {
        Q_UNUSED(workingDirectory);
        withActivationToken(platform_data, [this, &arguments]() { m_handleArgs(arguments); });
    }

private:
    std::function<void(const QStringList &)> m_handleArgs;
};

// Owns one of each adaptor; registered standalone at each of the two
// exported object paths (parity with KDBusService's live export).
class ActivationHost : public QObject
{
public:
    ActivationHost(std::function<void(const QStringList &)> handleArgs,
                   std::function<void(const QString &)> openUrl,
                   QObject *parent)
        : QObject(parent)
    {
        new ApplicationAdaptor(this, handleArgs, openUrl);
        new CommandLineAdaptor(this, std::move(handleArgs));
    }
};

} // namespace

bool registerActivation(std::function<void(const QStringList &)> handleArgs,
                        std::function<void(const QString &)> openUrl)
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        qWarning("Lane: no D-Bus session bus available, cannot register activation");
        return false;
    }

    if (!bus.registerService(QStringLiteral("app.lane.Lane"))) {
        qWarning("Lane: could not claim the app.lane.Lane bus name: %s", qPrintable(bus.lastError().message()));
        return false;
    }

    auto *mainObjectHost = new ActivationHost(handleArgs, openUrl, QCoreApplication::instance());
    auto *appObjectHost = new ActivationHost(handleArgs, openUrl, QCoreApplication::instance());

    const bool mainOk = bus.registerObject(QStringLiteral("/app/lane/Lane"), mainObjectHost, QDBusConnection::ExportAdaptors);
    const bool appOk = bus.registerObject(QStringLiteral("/MainApplication"), appObjectHost, QDBusConnection::ExportAdaptors);

    if (!mainOk || !appOk) {
        qWarning("Lane: could not register D-Bus activation objects");
        return false;
    }

    return true;
}

// Maps a .desktop [Desktop Action X] group key to the argv the action's
// Exec= line passes. Action names must stay in sync with Actions= and the
// group names in data/app.lane.Lane.desktop.in; test_kde_actions enforces it.
// argv[0] is always "lane": Controller::handleArgs strips index 0
// unconditionally, so it only ever sees the real arguments.
QStringList argsForAction(const QString &actionName)
{
    QStringList args{QStringLiteral("lane")};
    if (actionName == QLatin1String("Clipboard")) {
        args << QStringLiteral("--clipboard");
    } else if (actionName == QLatin1String("ClipboardPicker")) {
        args << QStringLiteral("--clipboard") << QStringLiteral("-p");
    } else if (actionName == QLatin1String("Settings")) {
        args << QStringLiteral("--settings");
    } else if (actionName == QLatin1String("Rescan")) {
        args << QStringLiteral("--rediscover");
    } else {
        // Unknown action: degrade to a plain Activate, but leave a trace so
        // a typo'd .desktop file or unmapped new action is diagnosable.
        // The name is caller-controlled; bound it before logging.
        qWarning("Lane: unknown D-Bus action '%s'; treating as plain activation",
                 qUtf8Printable(actionName.left(64)));
    }
    return args;
}
} // namespace Lane
#include "DBusActivation.moc"
