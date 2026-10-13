#include <QCoreApplication>
#include <QJsonObject>
#include <QLocalSocket>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include "../src/app/SingleInstance.h"

using namespace Lane;

// SingleInstance binds its socket under XDG_RUNTIME_DIR; each test
// points that at a fresh temporary dir so runs never see a real daemon
// or each other's sockets.
class SingleInstanceTest : public QObject
{
    Q_OBJECT

    QTemporaryDir m_runtime;

private Q_SLOTS:
    void init()
    {
        QVERIFY(m_runtime.isValid());
        qputenv("XDG_RUNTIME_DIR", m_runtime.path().toUtf8());
    }

    void cleanup()
    {
        qunsetenv("XDG_RUNTIME_DIR");
    }

    void primaryClaimsAndHolds()
    {
        SingleInstance first;
        QVERIFY(first.claim());
        // A second instance in the same process must forward, not claim.
        SingleInstance second;
        QSignalSpy spy(&second, &QObject::destroyed);
        bool delivered = false;
        first.onMessage = [&delivered](const QJsonObject &payload) {
            delivered = payload.contains(QStringLiteral("args"));
        };
        QCOMPARE(second.claim(), false);
        QTRY_VERIFY_WITH_TIMEOUT(delivered, 3000);
    }

    void staleSocketIsReclaimed()
    {
        // A socket file nobody listens on simulates a crashed daemon.
        SingleInstance first;
        QVERIFY(first.claim());
        const QString path = m_runtime.path() + QStringLiteral("/lane-instance.sock");
        // Destructing the QLocalServer unlinks the file, so hand-create
        first.deleteLater();
        qApp->processEvents();
        QVERIFY(QFile::remove(path));
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
        qputenv("XDG_ACTIVATION_TOKEN", "tok-123");
        SingleInstance first;
        QVERIFY(first.claim());
        QString gotToken;
        first.onMessage = [&gotToken](const QJsonObject &payload) {
            gotToken = payload.value(QStringLiteral("activationToken")).toString();
        };
        SingleInstance second;
        QCOMPARE(second.claim(), false);
        QTRY_COMPARE_WITH_TIMEOUT(gotToken, QStringLiteral("tok-123"), 3000);
        qunsetenv("XDG_ACTIVATION_TOKEN");
    }
};

QTEST_MAIN(SingleInstanceTest)
#include "test_singleinstance.moc"
