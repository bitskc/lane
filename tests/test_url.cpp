#include "core/urlutil.h"

#include <QTest>

class UrlTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void hostExtraction()
    {
        QCOMPARE(Tern::hostOf(QStringLiteral("https://GitHub.com/bitskc/tern")), QStringLiteral("github.com"));
        QCOMPARE(Tern::hostOf(QStringLiteral("github.com/foo")), QStringLiteral("github.com"));
    }

    void o365Unwrap()
    {
        const QString wrapped = QStringLiteral(
            "https://eur02.safelinks.protection.outlook.com/?url=https%3A%2F%2Fwww.google.com%2Fsearch%3Fq%3Dx&data=ignored");
        QCOMPARE(Tern::unwrapO365(wrapped), QStringLiteral("https://www.google.com/search?q=x"));
        QVERIFY(Tern::isO365Wrapper(wrapped));
        QVERIFY(!Tern::isO365Wrapper(QStringLiteral("https://github.com")));
    }

    void shortenerDetect()
    {
        QVERIFY(Tern::isShortener(QStringLiteral("https://bit.ly/abc")));
        QVERIFY(!Tern::isShortener(QStringLiteral("https://github.com")));
    }

    void pwaScope()
    {
        QVERIFY(Tern::urlInScope(QStringLiteral("https://github.com/bitskc/tern"), QStringLiteral("https://github.com/")));
        QVERIFY(!Tern::urlInScope(QStringLiteral("https://gitlab.com/x"), QStringLiteral("https://github.com/")));
        QVERIFY(Tern::urlInScope(QStringLiteral("https://qbo.intuit.com/app/homepage"),
                                 QStringLiteral("https://qbo.intuit.com/app/")));
        QVERIFY(!Tern::urlInScope(QStringLiteral("https://qbo.intuit.com/other"),
                                  QStringLiteral("https://qbo.intuit.com/app/")));
    }

    void safeOpenUrl()
    {
        QVERIFY(Tern::isSafeOpenUrl(QStringLiteral("https://github.com/x")));
        QVERIFY(Tern::isSafeOpenUrl(QStringLiteral("http://example.com")));
        QVERIFY(!Tern::isSafeOpenUrl(QStringLiteral("file:///etc/passwd")));
        QVERIFY(!Tern::isSafeOpenUrl(QStringLiteral("javascript:alert(1)")));
        QVERIFY(!Tern::isSafeOpenUrl(QStringLiteral("data:text/html,hi")));
        QVERIFY(!Tern::isSafeOpenUrl(QStringLiteral("https://user:pass@github.com")));
        QVERIFY(!Tern::isSafeOpenUrl(QStringLiteral("https://github.com/x\nhttps://evil.test")));
    }

    void privateHosts()
    {
        QVERIFY(Tern::isPrivateOrLocalHost(QStringLiteral("localhost")));
        QVERIFY(Tern::isPrivateOrLocalHost(QStringLiteral("127.0.0.1")));
        QVERIFY(Tern::isPrivateOrLocalHost(QStringLiteral("192.168.1.1")));
        QVERIFY(Tern::isPrivateOrLocalHost(QStringLiteral("10.0.0.5")));
        QVERIFY(Tern::isPrivateOrLocalHost(QStringLiteral("169.254.169.254")));
        QVERIFY(Tern::isPrivateOrLocalHost(QStringLiteral("nas.local")));
        QVERIFY(!Tern::isPrivateOrLocalHost(QStringLiteral("github.com")));
        QVERIFY(!Tern::isPrivateOrLocalHost(QStringLiteral("1.1.1.1")));
    }

    void displayStripsUserInfo()
    {
        const QString shown = Tern::displayUrl(QStringLiteral("https://user:secret@example.com/path"));
        QVERIFY(!shown.contains(QStringLiteral("secret")));
        QVERIFY(!shown.contains(QStringLiteral("user:")));
        QVERIFY(shown.contains(QStringLiteral("example.com")));
    }

    void o365RejectsUnsafeNested()
    {
        const QString wrapped = QStringLiteral(
            "https://nam.safelinks.protection.outlook.com/?url=file%3A%2F%2F%2Fetc%2Fpasswd");
        QCOMPARE(Tern::unwrapO365(wrapped), wrapped);
    }
};

QTEST_MAIN(UrlTest)
#include "test_url.moc"
