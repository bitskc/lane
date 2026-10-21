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
        // A scheme-less input whose query holds a URL is still a bare host.
        // "contains ://" is not a scheme test.
        QTest::newRow("bare host with url in query") << QStringLiteral("example.com/login?next=https://example.com/home")
                                                     << true
                                                     << QStringLiteral("http://example.com/login?next=https://example.com/home");
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

    // sanitizedOpenUrl feeds the browser launch and the unshorten result,
    // and pipeline.cpp feeds that result back into isSafeOpenUrl. Its output
    // has to survive that round trip. A PrettyDecoded toString() turned %20
    // into a literal space, which the strict parse then refuses.
    void sanitizedOutputRoundTrips_data()
    {
        QTest::addColumn<QString>("raw");
        QTest::newRow("encoded space") << QStringLiteral("https://example.com/a%20b");
        QTest::newRow("encoded space in query") << QStringLiteral("https://example.com/s?q=a%20b&r=2");
        QTest::newRow("sharepoint path")
            << QStringLiteral("https://contoso.sharepoint.com/sites/x/Shared%20Documents/a.docx");
        QTest::newRow("encoded json") << QStringLiteral("https://example.com/p?x=%7Bj%7D");
        QTest::newRow("unicode path") << QStringLiteral("https://example.com/%E6%97%A5%E6%9C%AC");
    }

    void sanitizedOutputRoundTrips()
    {
        QFETCH(QString, raw);
        const QString open = Lane::sanitizedOpenUrl(raw);
        QVERIFY2(!open.isEmpty(), "a safe URL must sanitize to something");
        // Every row is already canonical: the FullyEncoded rendering must
        // be byte-identical, so an over-encoder that also escapes '?',
        // '&', '#' or '/' fails here instead of just staying "safe".
        QCOMPARE(open, raw);
        QCOMPARE(Lane::sanitizedOpenUrl(open), open);
    }

    // A redirect target as the unshortener builds it: a QUrl rendered to a
    // string. The destination must be accepted as a destination.
    void redirectTargetWithEncodedSpaceIsSafe()
    {
        const QUrl loc(QStringLiteral("https://contoso.sharepoint.com/sites/x/Shared%20Documents/a.docx"));
        const QString candidate = Lane::redirectCandidate(loc);
        QVERIFY2(Lane::isSafeOpenUrl(candidate), qPrintable(candidate));
        QCOMPARE(Lane::hostOf(candidate), QStringLiteral("contoso.sharepoint.com"));
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

    // IPv6 transition prefixes carry an IPv4 address that the connection
    // actually reaches. Expected values come from RFC 6052 (NAT64
    // 64:ff9b::/96, IPv4 in the last 32 bits) and RFC 3056 (6to4 2002::/16,
    // IPv4 in bits 16 to 47), not from the implementation.
    void transitionPrefixes_data()
    {
        QTest::addColumn<QString>("host");
        QTest::addColumn<bool>("isPrivate");

        QTest::newRow("nat64 loopback") << QStringLiteral("64:ff9b::7f00:1") << true;
        QTest::newRow("nat64 rfc1918 10/8") << QStringLiteral("64:ff9b::a00:1") << true;
        QTest::newRow("nat64 rfc1918 192.168") << QStringLiteral("64:ff9b::c0a8:101") << true;
        QTest::newRow("nat64 link-local metadata") << QStringLiteral("64:ff9b::a9fe:a9fe") << true;
        QTest::newRow("nat64 public 1.1.1.1") << QStringLiteral("64:ff9b::101:101") << false;
        QTest::newRow("nat64 public 8.8.8.8") << QStringLiteral("64:ff9b::808:808") << false;

        QTest::newRow("6to4 loopback") << QStringLiteral("2002:7f00:1::") << true;
        QTest::newRow("6to4 rfc1918 10/8") << QStringLiteral("2002:a00:1::") << true;
        QTest::newRow("6to4 rfc1918 192.168") << QStringLiteral("2002:c0a8:101::") << true;
        // Offset discriminators: the correct read (bytes 2 to 5) and a
        // misread (bytes 4 to 7) land on opposite sides of the line, so an
        // off-by-two cannot hide behind addresses that classify the same.
        QTest::newRow("6to4 private, misread would be public") << QStringLiteral("2002:a00:808:808::") << true;
        QTest::newRow("6to4 public, misread would be private") << QStringLiteral("2002:808:7f00:1::") << false;
        QTest::newRow("6to4 link-local metadata") << QStringLiteral("2002:a9fe:a9fe::") << true;
        QTest::newRow("6to4 with a subnet and host part") << QStringLiteral("2002:7f00:1:5:1:2:3:4") << true;
        QTest::newRow("6to4 public 1.1.1.1") << QStringLiteral("2002:101:101::") << false;

        // Not decoded: 64:ff9b:1::/48 (RFC 8215) embeds the IPv4 differently,
        // so the /96 rule would read the wrong bytes. It is public IPv6 here.
        QTest::newRow("nat64 local-use prefix is not decoded") << QStringLiteral("64:ff9b:1::7f00:1") << false;
        // The /96 requires bytes 4 to 11 to be zero. A nonzero middle is not
        // the well-known prefix, so it must not be read as 127.0.0.1.
        QTest::newRow("nat64 nonzero middle is not decoded") << QStringLiteral("64:ff9b::1:0:7f00:1") << false;

        // Plain IPv6 is unchanged.
        QTest::newRow("ipv6 loopback") << QStringLiteral("::1") << true;
        QTest::newRow("ipv6 public") << QStringLiteral("2606:4700:4700::1111") << false;
    }

    void transitionPrefixes()
    {
        QFETCH(QString, host);
        QFETCH(bool, isPrivate);
        QCOMPARE(Lane::isPrivateOrLocalHost(host), isPrivate);
    }

    void displayStripsUserInfo()
    {
        const QString shown = Lane::displayUrl(QStringLiteral("https://user:secret@example.com/path"));
        QVERIFY(!shown.contains(QStringLiteral("secret")));
        QVERIFY(!shown.contains(QStringLiteral("user:")));
        QVERIFY(shown.contains(QStringLiteral("example.com")));
    }

    // Percent-encoded controls decode at display time. The label must not
    // carry a literal newline or a bidi/zero-width character; they come
    // back out percent-encoded so the label still matches what opens.
    void displayReencodesDecodedControls()
    {
        const QString nl = Lane::displayUrl(QStringLiteral("https://evil.example/%0AImportant%20notice"));
        QVERIFY(!nl.contains(QLatin1Char('\n')));
        QVERIFY(nl.contains(QStringLiteral("%0A")));
        QVERIFY(nl.contains(QStringLiteral("Important")));
        const QString rlo = Lane::displayUrl(QStringLiteral("https://evil.example/%E2%80%AEtxt.exe"));
        QVERIFY(!rlo.contains(QChar(0x202e)));
        QVERIFY(rlo.contains(QStringLiteral("%E2%80%AE")));
        QVERIFY(rlo.contains(QStringLiteral("txt.exe")));
    }

    void displayReencodesMarksZeroWidthAndBom()
    {
        const QChar bad[] = {QChar(0x200e), QChar(0x200f), QChar(0x200b), QChar(0x200c),
                             QChar(0x200d), QChar(0xfeff)};
        const QStringList enc{QStringLiteral("%E2%80%8E"), QStringLiteral("%E2%80%8F"),
                              QStringLiteral("%E2%80%8B"), QStringLiteral("%E2%80%8C"),
                              QStringLiteral("%E2%80%8D"), QStringLiteral("%EF%BB%BF")};
        for (int i = 0; i < enc.size(); ++i) {
            const QString shown = Lane::displayUrl(QStringLiteral("https://evil.example/a") + enc.at(i)
                                                   + QStringLiteral("b"));
            QVERIFY2(!shown.contains(bad[i]), qPrintable(enc.at(i)));
            QVERIFY2(shown.contains(enc.at(i)), qPrintable(shown));
        }
    }

    void o365RejectsUnsafeNested()
    {
        const QString wrapped = QStringLiteral(
            "https://nam.safelinks.protection.outlook.com/?url=file%3A%2F%2F%2Fetc%2Fpasswd");
        QCOMPARE(Lane::unwrapO365(wrapped), wrapped);
        // Scheme-less values: fromUserInput would invent http:// for these
        // and isSafeOpenUrl would then accept the fabricated URL, so only
        // the explicit-scheme gate keeps them wrapped. (An opaque SafeLinks
        // data= blob such as 05|01|user@contoso.com is NOT a gate test:
        // isSafeOpenUrl rejects it on its own, so it is not listed here.)
        const QString relativeUrl = QStringLiteral(
            "https://nam.safelinks.protection.outlook.com/?url=example.com%2Fpath");
        QCOMPARE(Lane::unwrapO365(relativeUrl), relativeUrl);
        const QString bareHostData = QStringLiteral(
            "https://nam.safelinks.protection.outlook.com/?data=example.com");
        QCOMPARE(Lane::unwrapO365(bareHostData), bareHostData);
        // Not a gate test, but the wrapper must still stay wrapped for it.
        const QString opaqueData = QStringLiteral(
            "https://nam.safelinks.protection.outlook.com/?data=05%7C01%7Cuser%40contoso.com");
        QCOMPARE(Lane::unwrapO365(opaqueData), opaqueData);
    }

    // The scheme gate is case-insensitive and tolerates surrounding
    // whitespace, since schemes are case-insensitive and wrappers pad.
    void o365SchemeGateAcceptsCaseAndWhitespace()
    {
        const QString upper = QStringLiteral(
            "https://nam.safelinks.protection.outlook.com/?url=HTTPS%3A%2F%2Fexample.com%2Fa");
        QVERIFY2(Lane::unwrapO365(upper) != upper, qPrintable(Lane::unwrapO365(upper)));
        QVERIFY(Lane::isSafeOpenUrl(Lane::unwrapO365(upper)));
        const QString padded = QStringLiteral(
            "https://nam.safelinks.protection.outlook.com/?url=%20https%3A%2F%2Fexample.com%2Fa%20");
        QCOMPARE(Lane::unwrapO365(padded), QStringLiteral("https://example.com/a"));
    }

    // SafeLinks nests the real URL percent-encoded. Decoding it fully turns
    // a %20 into a space, which is not a URL the strict check accepts, so the
    // wrapper stayed wrapped and the click routed as a Microsoft host.
    void o365UnwrapsNestedUrlWithEncodedSpace()
    {
        const QString wrapped = QStringLiteral(
            "https://nam.safelinks.protection.outlook.com/?url=https%3A%2F%2Fexample.com%2Fa%2520b&data=x");
        QCOMPARE(Lane::unwrapO365(wrapped), QStringLiteral("https://example.com/a%20b"));
        const QString literalSpace = QStringLiteral(
            "https://nam.safelinks.protection.outlook.com/?url=https%3A%2F%2Fexample.com%2Fa%20b&data=x");
        const QString unwrapped = Lane::unwrapO365(literalSpace);
        QCOMPARE(unwrapped, QStringLiteral("https://example.com/a%20b"));
        QVERIFY(Lane::isSafeOpenUrl(unwrapped));
    }
};

QTEST_MAIN(UrlTest)
#include "test_url.moc"
