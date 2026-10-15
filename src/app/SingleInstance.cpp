#include "SingleInstance.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <QLockFile>
#include <QStandardPaths>
#include <QTimer>

#include <cstdlib>
#include <memory>
#ifdef Q_OS_MACOS
#include <unistd.h>
#endif
#include <sys/stat.h>

namespace Lane
{
namespace
{

// Ack lines the primary writes back; see the protocol note in the header.
constexpr char kAckOk[] = "ok\n";
constexpr char kAckError[] = "err\n";

// A forwarded payload is a handful of argv entries. Anything past this
// is a client that will never complete a frame, so the primary drops the
// connection instead of buffering without bound.
constexpr qsizetype kMaxPayloadBytes = 64 * 1024;

constexpr int kConnectTimeoutMs = 500;
constexpr int kWriteTimeoutMs = 2000;
// The primary acks from its event loop, which at startup is still behind
// a discovery scan and an xdg-settings subprocess; wait that out rather
// than declaring a healthy daemon dead and rebinding over it.
constexpr int kAckTimeoutMs = 10000;
// Server side: a connection that never produces a frame is abandoned.
constexpr int kClientIdleTimeoutMs = 10000;
// Hold the lock only for connect/remove/bind, never for delivery/ack.
constexpr int kLockWaitMs = 5000;

[[noreturn]] void fatal(const QString &message)
{
    qCritical("%s", qPrintable(message));
    std::_Exit(1);
}

QString socketPath()
{
    const QString path = singleInstanceSocketPath();
    if (path.isEmpty()) {
        fatal(QStringLiteral("Lane: the instance socket path exceeds 103 bytes; refusing to pick a fallback"));
    }
    return path;
}


enum class ForwardResult {
    // The primary read the payload and acknowledged it.
    Forwarded,
    // Nothing is listening on the path: it is free (or stale) to bind.
    NoPrimary,
    // Something is listening but the handshake did not complete. The
    // caller must not rebind: that would unlink a live primary's socket.
    Failed,
};

// Sends our invocation to the primary listening on `path`. Called on the
// fast path before taking the claim lock, and again under it.
ForwardResult forwardToPrimary(const QString &path, QLockFile *claimLock = nullptr)
{
    QLocalSocket probe;
    probe.connectToServer(path);
    if (!probe.waitForConnected(kConnectTimeoutMs)) {
        const auto err = probe.error();
        if (err == QLocalSocket::ServerNotFoundError || err == QLocalSocket::ConnectionRefusedError)
            return ForwardResult::NoPrimary;
        // macOS reports ENOTSOCK (UnknownSocketError, errno 38) when a
        // stale regular file sits at the socket path, where Linux gives
        // ConnectionRefusedError. A path that exists but is not a socket
        // inode cannot have a live listener, so reclaiming is safe; this
        // check stays narrower than whitelisting UnknownSocketError.
        struct stat st;
        if (::stat(path.toLocal8Bit().constData(), &st) == 0 && !S_ISSOCK(st.st_mode))
            return ForwardResult::NoPrimary;
        qWarning("Lane: could not determine whether the primary is available: %s", qPrintable(probe.errorString()));
        return ForwardResult::Failed;
    }
    if (claimLock) claimLock->unlock();

    QJsonObject payload;
    payload.insert(QStringLiteral("args"), QJsonArray::fromStringList(QCoreApplication::arguments()));
    payload.insert(QStringLiteral("workingDir"), QDir::currentPath());
    payload.insert(QStringLiteral("activationToken"), qEnvironmentVariable("XDG_ACTIVATION_TOKEN"));
    const QByteArray frame = QJsonDocument(payload).toJson(QJsonDocument::Compact) + '\n';

    // write() buffers, so a short return or a failed flush is the only
    // way to see that the primary went away mid-write. Treating either
    // as success is how a URL invocation silently disappears.
    if (probe.write(frame) != frame.size()) {
        qWarning("Lane: could not queue the forwarded invocation: %s", qPrintable(probe.errorString()));
        return ForwardResult::Failed;
    }
    if (probe.bytesToWrite() > 0 && !probe.waitForBytesWritten(kWriteTimeoutMs)) {
        qWarning("Lane: could not write the forwarded invocation: %s", qPrintable(probe.errorString()));
        return ForwardResult::Failed;
    }

    // Drain first: the ack can already be buffered by the time the
    // write completes, and waiting for *more* data would then block
    // until the primary hangs up and report a healthy delivery as a
    // timeout.
    QByteArray ack = probe.readAll();
    while (!ack.contains('\n') && ack.size() <= qsizetype(sizeof(kAckError))) {
        const bool more = probe.waitForReadyRead(kAckTimeoutMs);
        ack.append(probe.readAll());
        if (!more) {
            break;
        }
    }

    probe.disconnectFromServer();
    if (probe.state() != QLocalSocket::UnconnectedState) {
        probe.waitForDisconnected(kConnectTimeoutMs);
    }

    if (ack.isEmpty()) {
        qWarning("Lane: the running instance never acknowledged the forwarded invocation: %s",
                  qPrintable(probe.errorString()));
        return ForwardResult::Failed;
    }
    if (ack != kAckOk) {
        qWarning("Lane: the running instance rejected the forwarded invocation");
        return ForwardResult::Failed;
    }
    return ForwardResult::Forwarded;
}

} // namespace

QString singleInstanceSocketPath()
{
    QString dir = qEnvironmentVariable("XDG_RUNTIME_DIR");
    if (dir.isEmpty()) {
#ifdef Q_OS_MACOS
        // Per-user temp dir via confstr, not TMPDIR: TMPDIR can be unset
        // or redirected per process. The directory ends with a slash.
        char dirbuf[4096];
        const size_t n = confstr(_CS_DARWIN_USER_TEMP_DIR, dirbuf, sizeof(dirbuf));
        if (n == 0 || n > sizeof(dirbuf)) {
            return {};
        }
        dir = QString::fromLocal8Bit(dirbuf, static_cast<qsizetype>(n - 1));
#else
        dir = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
#endif
    }
#ifdef Q_OS_MACOS
    if (!dir.endsWith(QLatin1Char('/'))) dir += QLatin1Char('/');
    const QString path = dir + QStringLiteral("lane-%1.sock").arg(getuid());
#else
    const QString path = dir + QLatin1String("/lane-instance.sock");
#endif
    // Darwin sockaddr_un sun_path is 104 bytes including the NUL, so the
    // name must fit in 103. No fallback path: binding somewhere else is
    // how two instances both think they are primary.
    if (path.toLocal8Bit().size() > 103) {
        return {};
    }
    return path;
}

SingleInstance::SingleInstance(QObject *parent)
    : QObject(parent)
{
}

bool SingleInstance::claim()
{
    const QString path = socketPath();

    // Do not listen() first: QLocalServer unlinks and rebinds an
    // existing filesystem socket path without checking whether a live
    // process is listening on it, which orphans the real daemon on an
    // unlinked inode and makes it unreachable. Probe the path first.
    ForwardResult forwarded = forwardToPrimary(path);
    if (forwarded == ForwardResult::Failed) {
        fatal(QStringLiteral("Lane: could not safely forward this invocation via %1; "
                              "refusing to replay it or replace the endpoint").arg(path));
    }
    if (forwarded == ForwardResult::Forwarded) {
        return false;
    }

    // Nobody answered, so the path is absent or stale and this process
    // intends to clear and rebind it. Two instances reaching this point
    // together would both unlink and rebind, and the loser of that race
    // keeps listening on an unlinked inode that no client can ever
    // reach again. Serialize clear-and-bind across processes.
    QLockFile lock(path + QLatin1String(".lock"));
    lock.setStaleLockTime(0); // A live owner's age never makes the lock stale.
    if (!lock.tryLock(kLockWaitMs)) {
        fatal(QStringLiteral("Lane: could not take the single-instance claim lock at %1.lock (error %2)")
                  .arg(path).arg(int(lock.error())));
    }

    // Re-probe under the lock: another instance may have finished
    // binding between the probe above and this point, and its socket
    // must survive.
    forwarded = forwardToPrimary(path, &lock);
    if (forwarded == ForwardResult::Failed) {
        fatal(QStringLiteral("Lane: could not safely forward this invocation via %1; "
                              "refusing to replay it or replace the endpoint").arg(path));
    }
    if (forwarded == ForwardResult::Forwarded) {
        return false;
    }

    QLocalServer::removeServer(path);

    m_server = new QLocalServer(this);
    m_server->setSocketOptions(QLocalServer::UserAccessOption);
    if (!m_server->listen(path)) {
        // The claim lock prevents another cooperating instance from
        // binding here. Never report a failed bind as a delivered URL.
        fatal(QStringLiteral("Lane: could not listen on the single-instance socket at %1: %2")
                  .arg(path, m_server->errorString()));
    }

    connect(m_server, &QLocalServer::newConnection, this, &SingleInstance::handleNewConnection);
    return true;
}

void SingleInstance::handleNewConnection()
{
    struct ClientState {
        QByteArray buffer;
        bool handled = false;
    };
    while (QLocalSocket *socket = m_server->nextPendingConnection()) {
        socket->setReadBufferSize(kMaxPayloadBytes + 1);
        auto state = std::make_shared<ClientState>();
        auto readFrame = [this, socket, state]() {
            if (state->handled) return;
            state->buffer.append(socket->read(kMaxPayloadBytes + 1 - state->buffer.size()));
            const qsizetype end = state->buffer.indexOf('\n');
            if (state->buffer.size() > kMaxPayloadBytes) {
                state->handled = true;
                socket->write(kAckError);
                socket->disconnectFromServer();
                return;
            }
            if (end < 0) return;
            state->handled = true;
            state->buffer.truncate(end);
            QJsonParseError error;
            const QJsonDocument doc = QJsonDocument::fromJson(state->buffer, &error);
            if (error.error != QJsonParseError::NoError || !doc.isObject() || !onMessage) {
                socket->write(kAckError);
                socket->disconnectFromServer();
                return;
            }
            // Acknowledge before dispatch: handleArgs can run a nested
            // event loop. Never touch the socket after onMessage, since
            // the nested loop may have processed its deferred deletion.
            socket->write(kAckOk);
            socket->flush();
            socket->disconnectFromServer();
            onMessage(doc.object());
        };
        connect(socket, &QLocalSocket::readyRead, socket, readFrame);
        connect(socket, &QLocalSocket::disconnected, socket, &QLocalSocket::deleteLater);
        connect(socket, &QLocalSocket::errorOccurred, socket, &QLocalSocket::deleteLater);
        QTimer::singleShot(kClientIdleTimeoutMs, socket, [socket, state]() {
            if (!state->handled) {
                socket->abort();
                socket->deleteLater();
            }
        });
        // Data can precede installing readyRead on an accepted socket.
        if (socket->bytesAvailable() > 0) readFrame();
    }
}

} // namespace Lane
