#include "SingleInstance.h"

#include <QAbstractSocket>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <QStandardPaths>

#include <cstdlib>

namespace Lane
{
namespace
{

QString socketPath()
{
    QString dir = qEnvironmentVariable("XDG_RUNTIME_DIR");
    if (dir.isEmpty()) {
        dir = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    }
    return dir + QLatin1String("/lane-instance.sock");
}

} // namespace

SingleInstance::SingleInstance(QObject *parent)
    : QObject(parent)
{
}

namespace
{

// Sends our invocation to the primary listening on `path` and returns
// true when it went through. Used twice: before the first bind attempt
// and again if bind loses the probe-then-listen race.
bool forwardToPrimary(const QString &path)
{
    QLocalSocket probe;
    probe.connectToServer(path);
    if (!probe.waitForConnected(500)) {
        return false;
    }
    QJsonObject payload;
    payload.insert(QStringLiteral("args"), QJsonArray::fromStringList(QCoreApplication::arguments()));
    payload.insert(QStringLiteral("workingDir"), QDir::currentPath());
    payload.insert(QStringLiteral("activationToken"), qEnvironmentVariable("XDG_ACTIVATION_TOKEN"));
    probe.write(QJsonDocument(payload).toJson(QJsonDocument::Compact));
    probe.waitForBytesWritten(500);
    probe.disconnectFromServer();
    if (probe.state() != QLocalSocket::UnconnectedState) {
        probe.waitForDisconnected(500);
    }
    return true;
}

} // namespace

bool SingleInstance::claim()
{
    m_server = new QLocalServer(this);
    m_server->setSocketOptions(QLocalServer::UserAccessOption);

    const QString path = socketPath();

    // Do not listen() first: QLocalServer unlinks and rebinds an
    // existing filesystem socket path without checking whether a live
    // process is listening on it, which orphans the real daemon on an
    // unlinked inode and makes it unreachable. Probe the path first.
    if (forwardToPrimary(path)) {
        return false;
    }
    // Nobody answered: the path is either absent or a stale file left
    // by a crashed daemon. Clear it before binding.
    QLocalServer::removeServer(path);

    if (!m_server->listen(path)) {
        // A listener could have appeared between our probe and bind;
        // give the forwarding path one more chance before failing hard.
        if (forwardToPrimary(path)) {
            return false;
        }
        qCritical("Lane: could not listen on the single-instance socket at %s: %s",
                  qPrintable(path), qPrintable(m_server->errorString()));
        std::exit(1);
    }

    connect(m_server, &QLocalServer::newConnection, this, &SingleInstance::handleNewConnection);
    return true;
}

void SingleInstance::handleNewConnection()
{
    while (QLocalSocket *socket = m_server->nextPendingConnection()) {
        connect(socket, &QLocalSocket::readyRead, this, [this, socket]() {
            // Forwarded payloads are a single small JSON-object write; a
            // short wait here covers the normal write-then-disconnect
            // race from the forwarding client without blocking the event
            // loop for long.
            socket->waitForReadyRead(200);
            const QByteArray data = socket->readAll();
            QJsonParseError err;
            const QJsonDocument doc = QJsonDocument::fromJson(data, &err);
            if (err.error == QJsonParseError::NoError && doc.isObject() && onMessage) {
                onMessage(doc.object());
            }
            socket->disconnectFromServer();
        });
        connect(socket, &QLocalSocket::disconnected, socket, &QLocalSocket::deleteLater);
    }
}

} // namespace Lane
