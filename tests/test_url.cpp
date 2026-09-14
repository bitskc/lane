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
