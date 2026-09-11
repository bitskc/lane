#include "core/matcher.h"

#include <QTest>

using namespace Tern;

class MatcherTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void domainSubstring()
    {
        Rule r;
        r.pattern = QStringLiteral("github");
        r.scope = MatchScope::Domain;
        Click c;
        c.matchUrl = QStringLiteral("https://github.com/foo");
        QVERIFY(ruleMatches(r, c));
        c.matchUrl = QStringLiteral("https://example.com/github");
        QVERIFY(!ruleMatches(r, c));
    }

    void pathSubstring()
    {
        Rule r;
        r.pattern = QStringLiteral("github");
        r.scope = MatchScope::Path;
        Click c;
        c.matchUrl = QStringLiteral("https://example.com/github/x");
        QVERIFY(ruleMatches(r, c));
        c.matchUrl = QStringLiteral("https://github.com/foo");
        QVERIFY(!ruleMatches(r, c));
    }

    void regexWholeInput()
    {
        Rule r;
        r.pattern = QStringLiteral(".*hub.*");
        r.regex = true;
        r.scope = MatchScope::Any;
        Click c;
        c.matchUrl = QStringLiteral("https://github.com");
        QVERIFY(ruleMatches(r, c));
        r.pattern = QStringLiteral("hub");
        QVERIFY(!ruleMatches(r, c));
    }

    void processName()
    {
        Rule r;
        r.pattern = QStringLiteral("slack");
        r.location = MatchLocation::ProcessName;
        Click c;
        c.matchUrl = QStringLiteral("https://example.com");
        c.processName = QStringLiteral("slack");
        QVERIFY(ruleMatches(r, c));
        c.processName = QStringLiteral("firefox");
        QVERIFY(!ruleMatches(r, c));
    }

    void disabled()
    {
        Rule r;
        r.pattern = QStringLiteral("github");
        r.enabled = false;
        Click c;
        c.matchUrl = QStringLiteral("https://github.com");
        QVERIFY(!ruleMatches(r, c));
    }

    void regexTooLong()
    {
        Rule r;
        r.regex = true;
        r.pattern = QString(200, QLatin1Char('a'));
        Click c;
        c.matchUrl = QStringLiteral("https://github.com");
        QVERIFY(!ruleMatches(r, c));
    }
};

QTEST_MAIN(MatcherTest)
#include "test_matcher.moc"
