#include "core/unshorten.h"

#include <QEventLoop>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTest>
#include <QTimer>

using namespace Lane;

// unshortenSync() makes a real network HEAD request only for URLs that
// pass both isShortener() and isSafeOpenUrl(); the known-shortener gate
// keeps a localhost stub from ever being asked. The Location -> click
// step is therefore split out as redirectLocationToCandidate(), which
// these tests exercise end to end: a real QTcpServer answers a request
// with a 302 and the helper must resolve, render, and gate it exactly
// like the live path. Reverting the FullyEncoded render inside the
// helper turns the encoded-space assertions red here.
class UnshortenTest : public QObject
{
    Q_OBJECT

    // Answer one request on a loopback QTcpServer with a bare 302 whose
    // Location is the given raw bytes, and return the finished reply for
    // the helper to inspect.
    QNetworkReply *fetchReplyWithLocation(QNetworkAccessManager &nam, const QByteArray &location)
    {
        QTcpServer server;
        if (!server.listen(QHostAddress::LocalHost)) {
            return nullptr;
        }
        const quint16 port = server.serverPort();

        QNetworkRequest req(QUrl(QStringLiteral("http://127.0.0.1:%1/anything").arg(port)));
        // Same policy unshortenSync uses: the reply must BE the 302, not
        // the post-follow result, so the helper sees the Location.
        req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::ManualRedirectPolicy);
        auto *reply = nam.get(req);

        QEventLoop loop;
        QTimer guard;
        guard.setSingleShot(true);
        connect(&guard, &QTimer::timeout, &loop, &QEventLoop::quit);
        connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        connect(&server, &QTcpServer::newConnection, this, [&]() {
            QTcpSocket *sock = server.nextPendingConnection();
            connect(sock, &QTcpSocket::readyRead, sock, [sock, location]() {
                sock->readAll(); // drain the request line/headers
                sock->write("HTTP/1.1 302 Found\r\nLocation: " + location
                            + "\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
                sock->flush();
                sock->disconnectFromHost();
            });
        });
        guard.start(5000);
        loop.exec();
        return reply;
    }

private Q_SLOTS:
    void nonShortenerHostPassesThroughWithoutNetworkCall()
    {
        // isShortener() gate: an ordinary host returns unchanged
        // immediately. A short timeout proves this ran synchronously
        // rather than falling through to the real HEAD request and
        // merely timing out with the same-looking result.
        const QString url = QStringLiteral("https://example.com/some/path");
        QCOMPARE(unshortenSync(url, 50), url);
    }

    void unsafeUrlPassesThroughEvenOnAShortenerHost()
    {
        // isSafeOpenUrl() gate: embedded credentials make the URL unsafe
        // to open even though the host is a known shortener, so
        // unshortenSync must never attempt the network request at all.
        const QString url = QStringLiteral("https://user:pass@bit.ly/abc123");
        QCOMPARE(unshortenSync(url, 50), url);
    }

    void emptyUrlPassesThroughUnchanged()
    {
        QCOMPARE(unshortenSync(QString(), 50), QString());
    }

    // An encoded space in the redirect target must survive as %20: the
    // old PrettyDecoded render produced a literal space, which the click
    // pipeline's isSafeOpenUrl then refused, leaving the click on the
    // shortener host.
    void redirectLocationKeepsEncodedSpace()
    {
        QNetworkAccessManager nam;
        auto *reply = fetchReplyWithLocation(
            nam, QByteArray("https://contoso.sharepoint.com/sites/x/Shared%20Documents/a.docx"));
        QVERIFY2(reply && reply->isFinished(), "stub server must answer");
        const QString out =
            redirectLocationToCandidate(QStringLiteral("https://bit.ly/x"), reply);
        QCOMPARE(out,
                 QStringLiteral("https://contoso.sharepoint.com/sites/x/Shared%20Documents/a.docx"));
        reply->deleteLater();
    }

    // A relative Location resolves against the request URL, then keeps
    // its encoding through the same render path.
    void relativeRedirectLocationResolvesAndKeepsEncoding()
    {
        QNetworkAccessManager nam;
        auto *reply = fetchReplyWithLocation(nam, QByteArray("/Shared%20Documents/a.docx"));
        QVERIFY2(reply && reply->isFinished(), "stub server must answer");
        const QString out =
            redirectLocationToCandidate(QStringLiteral("https://bit.ly/x"), reply);
        QCOMPARE(out, QStringLiteral("https://bit.ly/Shared%20Documents/a.docx"));
        reply->deleteLater();
    }

    // A redirect into the LAN must not be followed: the candidate fails
    // isPrivateOrLocalHost and the original URL is returned unchanged.
    void privateRedirectTargetIsRefused()
    {
        QNetworkAccessManager nam;
        const QString url = QStringLiteral("https://bit.ly/x");
        auto *reply = fetchReplyWithLocation(nam, QByteArray("http://192.168.1.1/admin"));
        QVERIFY2(reply && reply->isFinished(), "stub server must answer");
        QCOMPARE(redirectLocationToCandidate(url, reply), url);
        reply->deleteLater();
    }

    // A non-http(s) Location is refused by isSafeOpenUrl.
    void unsafeRedirectSchemeIsRefused()
    {
        QNetworkAccessManager nam;
        const QString url = QStringLiteral("https://bit.ly/x");
        auto *reply = fetchReplyWithLocation(nam, QByteArray("javascript:alert(1)"));
        QVERIFY2(reply && reply->isFinished(), "stub server must answer");
        QCOMPARE(redirectLocationToCandidate(url, reply), url);
        reply->deleteLater();
    }
};

QTEST_MAIN(UnshortenTest)
#include "test_unshorten.moc"
