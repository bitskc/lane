#include "core/urlutil.h"

#include <QTest>

class UrlTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void hostExtraction()
    {
        QCOMPARE(Lane::hostOf(QStringLiteral("https://GitHub.com/bitskc/lane")), QStringLiteral("github.com"));
        QCOMPARE(Lane::hostOf(QStringLiteral("github.com/foo")), QStringLiteral("github.com"));
    }

    void o365Unwrap()
    {
        const QString wrapped = QStringLiteral(
            "https://eur02.safelinks.protection.outlook.com/?url=https%3A%2F%2Fwww.google.com%2Fsearch%3Fq%3Dx&data=ignored");
        QCOMPARE(Lane::unwrapO365(wrapped), QStringLiteral("https://www.google.com/search?q=x"));
        QVERIFY(Lane::isO365Wrapper(wrapped));
        QVERIFY(!Lane::isO365Wrapper(QStringLiteral("https://github.com")));
    }

    void shortenerDetect()
    {
        QVERIFY(Lane::isShortener(QStringLiteral("https://bit.ly/abc")));
        QVERIFY(!Lane::isShortener(QStringLiteral("https://github.com")));
    }

    void pwaScope()
    {
        QVERIFY(Lane::urlInScope(QStringLiteral("https://github.com/bitskc/lane"), QStringLiteral("https://github.com/")));
        QVERIFY(!Lane::urlInScope(QStringLiteral("https://gitlab.com/x"), QStringLiteral("https://github.com/")));
        QVERIFY(Lane::urlInScope(QStringLiteral("https://qbo.intuit.com/app/homepage"),
                                 QStringLiteral("https://qbo.intuit.com/app/")));
        QVERIFY(!Lane::urlInScope(QStringLiteral("https://qbo.intuit.com/other"),
                                  QStringLiteral("https://qbo.intuit.com/app/")));
        // Segment boundary, not a raw string prefix: a scope of "/bits"
        // must not match a path that merely starts with those characters
        // ("/bitskc/lane" is a different first segment, not a subpath).
        QVERIFY(!Lane::urlInScope(QStringLiteral("https://example.com/bitskc/lane"),
                                  QStringLiteral("https://example.com/bits")));
        QVERIFY(Lane::urlInScope(QStringLiteral("https://example.com/bits/sub"),
                                 QStringLiteral("https://example.com/bits")));
        QVERIFY(Lane::urlInScope(QStringLiteral("https://example.com/bits"),
                                 QStringLiteral("https://example.com/bits")));
    }

    void safeOpenUrl()
    {
        QVERIFY(Lane::isSafeOpenUrl(QStringLiteral("https://github.com/x")));
        QVERIFY(Lane::isSafeOpenUrl(QStringLiteral("http://example.com")));
        QVERIFY(!Lane::isSafeOpenUrl(QStringLiteral("file:///etc/passwd")));
        QVERIFY(!Lane::isSafeOpenUrl(QStringLiteral("javascript:alert(1)")));
        QVERIFY(!Lane::isSafeOpenUrl(QStringLiteral("data:text/html,hi")));
        QVERIFY(!Lane::isSafeOpenUrl(QStringLiteral("https://user:pass@github.com")));
        QVERIFY(!Lane::isSafeOpenUrl(QStringLiteral("https://github.com/x\nhttps://evil.test")));
    }

    // The real entry path is normalizeInput then isSafeOpenUrl (pipeline.cpp
    // normalizes the raw click, router.cpp gates the result). Testing
    // isSafeOpenUrl alone passed while normalizeInput repaired the attack
    // strings into a URL whose host was "https".
    void clickChain_data()
    {
        QTest::addColumn<QString>("raw");
        QTest::addColumn<bool>("safe");
        QTest::addColumn<QString>("expected"); // empty = not asserted

        // Must stay refused after normalization.
        QTest::newRow("trailing dots") << QStringLiteral("https://example.com..") << false << QString();
        QTest::newRow("port too large") << QStringLiteral("http://example.com:99999/") << false << QString();
        QTest::newRow("negative port") << QStringLiteral("http://example.com:-1/") << false << QString();
        QTest::newRow("userinfo") << QStringLiteral("https://user:pw@example.com/") << false << QString();
        QTest::newRow("javascript") << QStringLiteral("javascript:alert(1)") << false << QString();
        QTest::newRow("file") << QStringLiteral("file:///etc/passwd") << false << QString();
        QTest::newRow("newline") << QStringLiteral("https://example.com/a\nb") << false << QString();
        QTest::newRow("carriage return") << QStringLiteral("https://example.com/a\rb") << false << QString();
        QTest::newRow("nul") << QString(QStringLiteral("https://example.com/a") + QChar(0) + QStringLiteral("b"))
                             << false << QString();
        QTest::newRow("line separator")
            << QString(QStringLiteral("https://example.com/") + QChar(0x2028) + QStringLiteral("x")) << false
            << QString();
        QTest::newRow("paragraph separator")
            << QString(QStringLiteral("https://example.com/") + QChar(0x2029) + QStringLiteral("x")) << false
            << QString();

        // Must still open, fully encoded.
        QTest::newRow("plain") << QStringLiteral("https://github.com/x") << true
                               << QStringLiteral("https://github.com/x");
        QTest::newRow("encoded space stays encoded") << QStringLiteral("https://example.com/a%20b") << true
                                                     << QStringLiteral("https://example.com/a%20b");
        QTest::newRow("literal space is encoded") << QStringLiteral("https://example.com/path with spaces") << true
                                                  << QStringLiteral("https://example.com/path%20with%20spaces");
        QTest::newRow("literal space in query") << QStringLiteral("https://example.com/s?q=hello world") << true
                                                << QStringLiteral("https://example.com/s?q=hello%20world");
        QTest::newRow("stray percent") << QStringLiteral("https://example.com/100%off") << true
                                       << QStringLiteral("https://example.com/100%25off");
        QTest::newRow("idn host") << QString::fromUtf8("https://b\xC3\xBC" "cher.example/") << true
                                  << QStringLiteral("https://xn--bcher-kva.example/");
        QTest::newRow("unicode path") << QString::fromUtf8("https://example.com/\xE6\x97\xA5\xE6\x9C\xAC") << true
                                      << QStringLiteral("https://example.com/%E6%97%A5%E6%9C%AC");
        QTest::newRow("ipv6 literal with port") << QStringLiteral("http://[::1]:8080/x") << true
                                                << QStringLiteral("http://[::1]:8080/x");
        QTest::newRow("port and encoded json") << QStringLiteral("https://example.com:8443/p?x=%7Bj%7D") << true
                                               << QStringLiteral("https://example.com:8443/p?x=%7Bj%7D");
        QTest::newRow("query and fragment") << QStringLiteral("https://a.b/?q=1&r=2#frag") << true
                                            << QStringLiteral("https://a.b/?q=1&r=2#frag");
        QTest::newRow("bare host gets a scheme") << QStringLiteral("example.com/x") << true
                                                 << QStringLiteral("http://example.com/x");
        QTest::newRow("uppercase scheme and host") << QStringLiteral("HTTPS://EXAMPLE.COM/X") << true
                                                   << QStringLiteral("https://example.com/X");
    }

    void clickChain()
    {
        QFETCH(QString, raw);
        QFETCH(bool, safe);
        QFETCH(QString, expected);
        const QString normalized = Lane::normalizeInput(raw);
        QCOMPARE(Lane::isSafeOpenUrl(normalized), safe);
        if (!expected.isEmpty()) {
            QCOMPARE(normalized, expected);
        }
        if (!safe) {
            // A refused click must never yield an openable string, and the
            // normalizer must not have invented a host out of the scheme.
            QVERIFY(Lane::sanitizedOpenUrl(normalized).isEmpty());
            QVERIFY(Lane::hostOf(normalized) != QLatin1String("https"));
            QVERIFY(Lane::hostOf(normalized) != QLatin1String("http"));
        }
    }

    void privateHosts()
    {
        QVERIFY(Lane::isPrivateOrLocalHost(QStringLiteral("localhost")));
        QVERIFY(Lane::isPrivateOrLocalHost(QStringLiteral("127.0.0.1")));
        QVERIFY(Lane::isPrivateOrLocalHost(QStringLiteral("192.168.1.1")));
        QVERIFY(Lane::isPrivateOrLocalHost(QStringLiteral("10.0.0.5")));
        QVERIFY(Lane::isPrivateOrLocalHost(QStringLiteral("169.254.169.254")));
        QVERIFY(Lane::isPrivateOrLocalHost(QStringLiteral("nas.local")));
        QVERIFY(!Lane::isPrivateOrLocalHost(QStringLiteral("github.com")));
        QVERIFY(!Lane::isPrivateOrLocalHost(QStringLiteral("1.1.1.1")));
        // RFC2544 benchmarking range is 198.18.0.0/15 (198.18.x and
        // 198.19.x), not all of 198.18.x/198.51.x.
        QVERIFY(Lane::isPrivateOrLocalHost(QStringLiteral("198.18.0.1")));
        QVERIFY(Lane::isPrivateOrLocalHost(QStringLiteral("198.19.255.254")));
        QVERIFY(!Lane::isPrivateOrLocalHost(QStringLiteral("198.20.0.1")));
        // TEST-NET-2 is only the 198.51.100.0/24 slice of 198.51.x.
        QVERIFY(Lane::isPrivateOrLocalHost(QStringLiteral("198.51.100.7")));
        QVERIFY(!Lane::isPrivateOrLocalHost(QStringLiteral("198.51.50.7")));
        // DNS absolute form. A trailing dot must not make these public.
        QVERIFY(Lane::isPrivateOrLocalHost(QStringLiteral("localhost.")));
        QVERIFY(Lane::isPrivateOrLocalHost(QStringLiteral("127.0.0.1.")));
        QVERIFY(Lane::isPrivateOrLocalHost(QStringLiteral("2130706433.")));
        QVERIFY(Lane::isPrivateOrLocalHost(QStringLiteral("metadata.google.internal.")));
        QVERIFY(Lane::isPrivateOrLocalHost(QStringLiteral("nas.local.")));
        QVERIFY(!Lane::isPrivateOrLocalHost(QStringLiteral("1.1.1.1.")));
        QVERIFY(Lane::isPrivateOrLocalHost(QString(QStringLiteral("127.0.0.1")) + QChar(0x3002)));
    }

    void displayStripsUserInfo()
    {
        const QString shown = Lane::displayUrl(QStringLiteral("https://user:secret@example.com/path"));
        QVERIFY(!shown.contains(QStringLiteral("secret")));
        QVERIFY(!shown.contains(QStringLiteral("user:")));
        QVERIFY(shown.contains(QStringLiteral("example.com")));
    }

    void o365RejectsUnsafeNested()
    {
        const QString wrapped = QStringLiteral(
            "https://nam.safelinks.protection.outlook.com/?url=file%3A%2F%2F%2Fetc%2Fpasswd");
        QCOMPARE(Lane::unwrapO365(wrapped), wrapped);
    }
};

QTEST_MAIN(UrlTest)
#include "test_url.moc"
