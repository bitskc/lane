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
};

QTEST_MAIN(UrlTest)
#include "test_url.moc"
