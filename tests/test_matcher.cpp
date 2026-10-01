#include "core/matcher.h"

#include <QTest>

using namespace Lane;

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

    void activityGate()
    {
        // ruleActivityMatches is the pure gate; callers pass the current
        // Activity explicitly so no KActivities service is needed here.
        Rule r;
        r.pattern = QStringLiteral("github");
        r.activity = QStringLiteral("act-work");

        // Unscoped rules pass in every Activity and with none at all.
        Rule unscoped;
        QVERIFY(ruleActivityMatches(unscoped, QStringLiteral("act-work"), QStringLiteral("Work")));
        QVERIFY(ruleActivityMatches(unscoped, QString(), QString()));

        // Scoped rules match by ID or by name, only while an Activity is
        // actually known; with no current Activity nothing is filtered,
        // which is what keeps non-Plasma desktops identical to before.
        QVERIFY(ruleActivityMatches(r, QStringLiteral("act-work"), QStringLiteral("Work")));
        QVERIFY(ruleActivityMatches(r, QStringLiteral("act-work"), QString()));
        // The rule value is compared against both the current Activity's
        // ID and its name: matching either one counts, matching neither
        // does not.
        QVERIFY(ruleActivityMatches(r, QStringLiteral("other-id"), QStringLiteral("act-work")));
        QVERIFY(!ruleActivityMatches(r, QStringLiteral("other-id"), QStringLiteral("Work")));
        QVERIFY(!ruleActivityMatches(r, QStringLiteral("act-fun"), QStringLiteral("Personal")));
        QVERIFY(ruleActivityMatches(r, QString(), QString()));
    }

    void activityScopedRuleThroughClick()
    {
        Rule r;
        r.pattern = QStringLiteral("github");
        r.scope = MatchScope::Domain;
        r.activity = QStringLiteral("act-work");
        Click c;
        c.matchUrl = QStringLiteral("https://github.com/foo");

        // Same click, different Activity context: the rule only matches
        // while its Activity is current.
        c.activityId = QStringLiteral("act-work");
        QVERIFY(ruleMatches(r, c));
        c.activityId = QStringLiteral("act-fun");
        QVERIFY(!ruleMatches(r, c));
        c.activityId.clear();
        QVERIFY(ruleMatches(r, c));

        // Name matching: a rule written against the Activity's display
        // name matches when the click carries that name.
        r.activity = QStringLiteral("Work");
        c.activityId = QStringLiteral("act-work");
        c.activityName = QStringLiteral("Work");
        QVERIFY(ruleMatches(r, c));
        c.activityName = QStringLiteral("Personal");
        QVERIFY(!ruleMatches(r, c));
    }
};

QTEST_MAIN(MatcherTest)
#include "test_matcher.moc"
