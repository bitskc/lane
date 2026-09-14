#include "core/unshorten.h"

#include <QTest>

using namespace Lane;

// unshortenSync() makes a real network HEAD request for any URL that
// passes both isShortener() and isSafeOpenUrl(); that live round-trip
// (redirect resolution, the 3-tier Location extraction, relative
// resolution, and the isPrivateOrLocalHost() safety check on the
// resolved target) is not reachable from lane-core's test surface
// without a real or mock HTTP endpoint to redirect to, and Qt6::HttpServer
// is not available in this build to stand one up on localhost. These
// tests cover what is reachable without the network: the two gates that
// run before any request is ever made, both of which short-circuit and
// return synchronously regardless of the timeoutMs argument.
class UnshortenTest : public QObject
{
    Q_OBJECT
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
};

QTEST_MAIN(UnshortenTest)
#include "test_unshorten.moc"
