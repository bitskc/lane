#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QLocalSocket>
#include <QSemaphore>
#include <QStringList>
#include <QTemporaryDir>
#include <QTest>
#include <QThread>

#include <atomic>

#include <sys/stat.h>
#ifdef Q_OS_MACOS
#include <unistd.h>
#endif

#include "../src/app/SingleInstance.h"

using namespace Lane;

namespace
{

// A forwarding invocation is a separate process in production: it blocks
// on its own socket while the primary's event loop accepts, reads and
// acknowledges. Running claim() on a worker thread reproduces that
// relationship; calling it inline would deadlock, because the thread
// waiting for the ack is the only one that could deliver it.
class ClaimRunner : public QThread
{
public:
    explicit ClaimRunner(QSemaphore *gate = nullptr)
        : m_gate(gate)
    {
    }

    ~ClaimRunner() override
    {
        quit();
        wait(5000);
    }

    void run() override
    {
        SingleInstance instance;
        instance.onMessage = [this](const QJsonObject &payload) {
            m_token = payload.value(QStringLiteral("activationToken")).toString();
            m_args = payload.value(QStringLiteral("args")).toArray().size();
            received = true;
        };
        if (m_gate) {
            m_gate->acquire();
        }
        claimed = instance.claim();
        if (claimed) {
            // Primary: serve forwarded payloads until the test stops us.
            exec();
        }
        done = true;
    }

    std::atomic<bool> claimed{false};
    std::atomic<bool> received{false};
    std::atomic<bool> done{false};

    // Written in run(), read only after the thread has been joined.
    QString token() const { return m_token; }
    int argCount() const { return m_args; }

private:
    QSemaphore *m_gate = nullptr;
    QString m_token;
    int m_args = 0;
};

QByteArray frameFor(const QJsonObject &payload)
{
    return QJsonDocument(payload).toJson(QJsonDocument::Compact) + '\n';
}

// Inode identity of the bound socket: the point of the claim lock is
// that a live primary's socket file is never unlinked and rebound.
ino_t inodeOf(const QString &path)
{
    struct stat st = {};
    if (::stat(path.toUtf8().constData(), &st) != 0) {
        return 0;
    }
    return st.st_ino;
}

} // namespace

// SingleInstance binds its socket under XDG_RUNTIME_DIR; each test
// points that at a fresh temporary dir so runs never see a real daemon
// or each other's sockets.
class SingleInstanceTest : public QObject
{
    Q_OBJECT

    QTemporaryDir m_runtime;

    QString socketPath() const { return singleInstanceSocketPath(); }

private Q_SLOTS:
    void init()
    {
        QVERIFY(m_runtime.isValid());
        qputenv("XDG_RUNTIME_DIR", m_runtime.path().toUtf8());
    }

    void cleanup()
    {
        qunsetenv("XDG_RUNTIME_DIR");
        QFile::remove(socketPath());
        QFile::remove(socketPath() + QStringLiteral(".lock"));
    }

    void primaryClaimsAndHolds()
    {
        SingleInstance first;
        bool delivered = false;
        first.onMessage = [&delivered](const QJsonObject &payload) {
            delivered = payload.contains(QStringLiteral("args"));
        };
        QVERIFY(first.claim());
        const ino_t bound = inodeOf(socketPath());
        QVERIFY(bound != 0);

        // A second instance must forward and report "not primary".
        ClaimRunner second;
        second.start();
        QTRY_VERIFY_WITH_TIMEOUT(second.done.load(), 15000);
        QVERIFY(second.wait(5000));
        QCOMPARE(second.claimed.load(), false);
        QVERIFY(delivered);

        // The live primary's socket must still be the one on disk: a
        // forwarding instance that unlinks and rebinds leaves the real
        // daemon listening on an orphaned inode.
        QCOMPARE(inodeOf(socketPath()), bound);
    }

    void concurrentClaimsYieldExactlyOnePrimary()
    {
        // Both threads probe an empty path at the same time. Without a
        // serialized clear-and-bind both bind: the loser unlinks the
        // winner's socket and keeps listening on an unreachable inode.
        QSemaphore gate;
        ClaimRunner a(&gate);
        ClaimRunner b(&gate);
        a.start();
        b.start();
        QTest::qWait(50);
        gate.release(2);

        QTRY_VERIFY_WITH_TIMEOUT(a.done.load() || b.done.load(), 20000);
        ClaimRunner &loser = a.done.load() && !a.claimed.load() ? a : b;
        ClaimRunner &winner = (&loser == &a) ? b : a;
        QTRY_VERIFY_WITH_TIMEOUT(loser.done.load(), 20000);
        QVERIFY(loser.wait(5000));

        QCOMPARE(loser.claimed.load(), false);
        QTRY_VERIFY_WITH_TIMEOUT(winner.claimed.load(), 5000);
        // The forwarded invocation reached the winner rather than
        // vanishing into an orphaned socket.
        QTRY_VERIFY_WITH_TIMEOUT(winner.received.load(), 5000);

        winner.quit();
        QVERIFY(winner.wait(5000));
    }

    void staleSocketIsReclaimed()
    {
        auto *first = new SingleInstance;
        QVERIFY(first->claim());
        const QString path = socketPath();
        QVERIFY(QFile::exists(path));

        // Destroying the server closes and unlinks the socket; a plain
        // file left in its place is what a crashed daemon leaves behind.
        delete first;
        if (QFile::exists(path)) {
            QVERIFY(QFile::remove(path));
        }
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.close();

        SingleInstance second;
        QVERIFY(second.claim());

        // And the recovered listener really is live.
        QLocalSocket client;
        client.connectToServer(path);
        QVERIFY(client.waitForConnected(1000));
        client.disconnectFromServer();
    }

    void forwardedPayloadCarriesToken()
    {
        SingleInstance first;
        QString gotToken;
        first.onMessage = [&gotToken](const QJsonObject &payload) {
            gotToken = payload.value(QStringLiteral("activationToken")).toString();
        };
        QVERIFY(first.claim());

        qputenv("XDG_ACTIVATION_TOKEN", "tok-123");
        ClaimRunner second;
        second.start();
        QTRY_VERIFY_WITH_TIMEOUT(second.done.load(), 15000);
        QVERIFY(second.wait(5000));
        qunsetenv("XDG_ACTIVATION_TOKEN");

        QCOMPARE(second.claimed.load(), false);
        QCOMPARE(gotToken, QStringLiteral("tok-123"));
    }

    void payloadSplitAcrossWritesIsDelivered()
    {
        SingleInstance first;
        QStringList gotArgs;
        first.onMessage = [&gotArgs](const QJsonObject &payload) {
            for (const QJsonValue &v : payload.value(QStringLiteral("args")).toArray()) {
                gotArgs << v.toString();
            }
        };
        QVERIFY(first.claim());

        QJsonObject payload;
        payload.insert(QStringLiteral("args"),
                        QJsonArray{QStringLiteral("lane"), QStringLiteral("https://example.com/")});
        const QByteArray frame = frameFor(payload);

        QLocalSocket client;
        client.connectToServer(socketPath());
        QVERIFY(client.waitForConnected(1000));
        // Half a frame must not be parsed as a whole one, and must not
        // be dropped either.
        client.write(frame.left(frame.size() / 2));
        client.flush();
        QTest::qWait(100);
        QVERIFY(gotArgs.isEmpty());
        client.write(frame.mid(frame.size() / 2));
        client.flush();

        QTRY_COMPARE_WITH_TIMEOUT(gotArgs.size(), 2, 5000);
        QCOMPARE(gotArgs.at(1), QStringLiteral("https://example.com/"));
        QTRY_VERIFY_WITH_TIMEOUT(client.bytesAvailable() > 0, 5000);
        QVERIFY(client.readAll().startsWith("ok"));
    }

    void malformedPayloadIsRejectedNotIgnored()
    {
        SingleInstance first;
        bool delivered = false;
        first.onMessage = [&delivered](const QJsonObject &) { delivered = true; };
        QVERIFY(first.claim());

        QLocalSocket client;
        client.connectToServer(socketPath());
        QVERIFY(client.waitForConnected(1000));
        client.write("{ not json \n");
        client.flush();

        // The sender is told the payload was rejected instead of
        // exiting 0 on a message nobody acted on.
        QTRY_VERIFY_WITH_TIMEOUT(client.bytesAvailable() > 0, 5000);
        QVERIFY(client.readAll().startsWith("err"));
        QVERIFY(!delivered);
    }

    void oversizedPayloadIsDropped()
    {
        SingleInstance first;
        bool delivered = false;
        first.onMessage = [&delivered](const QJsonObject &) { delivered = true; };
        QVERIFY(first.claim());

        QLocalSocket client;
        client.connectToServer(socketPath());
        QVERIFY(client.waitForConnected(1000));
        // No frame terminator, more than the payload cap: the primary
        // must not buffer it without bound.
        client.write(QByteArray(80 * 1024, 'x'));
        client.flush();

        QTRY_VERIFY_WITH_TIMEOUT(client.bytesAvailable() > 0 || client.state() == QLocalSocket::UnconnectedState, 5000);
        QVERIFY(!delivered);
    }

#ifdef Q_OS_MACOS
    void socketPathUsesConfstrTempDir()
    {
        // With the test override cleared, the path must come from
        // confstr(_CS_DARWIN_USER_TEMP_DIR) and carry the uid.
        qunsetenv("XDG_RUNTIME_DIR");
        const QString path = singleInstanceSocketPath();
        QVERIFY2(!path.isEmpty(), "real temp-dir path must fit in 103 bytes");
        QVERIFY(path.contains(QStringLiteral("lane-%1.sock").arg(getuid())));
        QVERIFY(path.startsWith(QStringLiteral("/var/folders/")));
        // Restore the sandbox for cleanup().
        qputenv("XDG_RUNTIME_DIR", m_runtime.path().toUtf8());
    }

    void socketPathOverlongFailsInsteadOfFallingBack()
    {
        // A base dir that pushes the socket name past 103 bytes must
        // yield an empty path (caller fails), not a relocated socket.
        QTemporaryDir base;
        QVERIFY(base.isValid());
        qputenv("XDG_RUNTIME_DIR",
                (base.path() + QLatin1Char('/') + QString(110, QLatin1Char('y'))).toUtf8());
        QVERIFY(singleInstanceSocketPath().isEmpty());
    }
#endif
};

QTEST_MAIN(SingleInstanceTest)
#include "test_singleinstance.moc"
