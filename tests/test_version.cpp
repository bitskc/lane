#include "core/updatemessages.h"
#include "core/version.h"

#include <QTest>

class VersionTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void sameVersion()
    {
        QCOMPARE(static_cast<int>(Lane::compareVersions(QStringLiteral("0.1.0"), QStringLiteral("0.1.0"))),
                 static_cast<int>(Lane::VersionOrder::Same));
    }

    void vPrefixIgnored()
    {
        QCOMPARE(static_cast<int>(Lane::compareVersions(QStringLiteral("v0.1.0"), QStringLiteral("0.1.0"))),
                 static_cast<int>(Lane::VersionOrder::Same));
        QCOMPARE(static_cast<int>(Lane::compareVersions(QStringLiteral("0.1.0"), QStringLiteral("V0.1.0"))),
                 static_cast<int>(Lane::VersionOrder::Same));
    }

    void unequalSegmentCounts()
    {
        // "0.2" has no patch segment; it is still newer than "0.1.0".
        QCOMPARE(static_cast<int>(Lane::compareVersions(QStringLiteral("0.2"), QStringLiteral("0.1.0"))),
                 static_cast<int>(Lane::VersionOrder::Newer));
        QCOMPARE(static_cast<int>(Lane::compareVersions(QStringLiteral("0.1.0"), QStringLiteral("0.2"))),
                 static_cast<int>(Lane::VersionOrder::Older));
        // Missing trailing segments are zero, not "less precise".
        QCOMPARE(static_cast<int>(Lane::compareVersions(QStringLiteral("1"), QStringLiteral("1.0.0"))),
                 static_cast<int>(Lane::VersionOrder::Same));
    }

    void numericSegmentsNotLexicographic()
    {
        // A naive string compare would say "0.10.0" < "0.9.0" because '1' < '9'.
        QCOMPARE(static_cast<int>(Lane::compareVersions(QStringLiteral("0.10.0"), QStringLiteral("0.9.0"))),
                 static_cast<int>(Lane::VersionOrder::Newer));
        QCOMPARE(static_cast<int>(Lane::compareVersions(QStringLiteral("0.9.0"), QStringLiteral("0.10.0"))),
                 static_cast<int>(Lane::VersionOrder::Older));
    }

    void prereleaseSortsBeforeRelease()
    {
        QCOMPARE(static_cast<int>(Lane::compareVersions(QStringLiteral("0.2.0-rc1"), QStringLiteral("0.2.0"))),
                 static_cast<int>(Lane::VersionOrder::Older));
        QCOMPARE(static_cast<int>(Lane::compareVersions(QStringLiteral("0.2.0"), QStringLiteral("0.2.0-rc1"))),
                 static_cast<int>(Lane::VersionOrder::Newer));
        // Two prereleases of the same version compare identifier by identifier.
        QCOMPARE(static_cast<int>(Lane::compareVersions(QStringLiteral("0.2.0-rc1"), QStringLiteral("0.2.0-rc2"))),
                 static_cast<int>(Lane::VersionOrder::Older));
        QCOMPARE(static_cast<int>(Lane::compareVersions(QStringLiteral("0.2.0-alpha"), QStringLiteral("0.2.0-beta"))),
                 static_cast<int>(Lane::VersionOrder::Older));
    }

    void garbageInputIsUnknown()
    {
        QCOMPARE(static_cast<int>(Lane::compareVersions(QStringLiteral("not-a-version"), QStringLiteral("0.1.0"))),
                 static_cast<int>(Lane::VersionOrder::Unknown));
        QCOMPARE(static_cast<int>(Lane::compareVersions(QStringLiteral("0.1.0"), QStringLiteral(""))),
                 static_cast<int>(Lane::VersionOrder::Unknown));
        QCOMPARE(static_cast<int>(Lane::compareVersions(QStringLiteral("1.a.0"), QStringLiteral("1.0.0"))),
                 static_cast<int>(Lane::VersionOrder::Unknown));
        QCOMPARE(static_cast<int>(Lane::compareVersions(QStringLiteral(""), QStringLiteral(""))),
                 static_cast<int>(Lane::VersionOrder::Unknown));
        QCOMPARE(static_cast<int>(Lane::compareVersions(QStringLiteral("1.0.0-"), QStringLiteral("1.0.0"))),
                 static_cast<int>(Lane::VersionOrder::Unknown));
    }

    // Each distinct update-check failure must read differently, so a user
    // (and whoever is debugging a report) can tell "offline" from "GitHub
    // is down" from "GitHub sent something unexpected" instead of all five
    // collapsing to the same generic sentence.
    void updateFailureCausesReadDifferently()
    {
        const QString hostNotFound = Lane::describeNetworkError(QNetworkReply::HostNotFoundError);
        const QString refused = Lane::describeNetworkError(QNetworkReply::ConnectionRefusedError);
        const QString timedOut = Lane::describeNetworkError(QNetworkReply::TimeoutError);
        QVERIFY(hostNotFound != refused);
        QVERIFY(hostNotFound != timedOut);
        QVERIFY(refused != timedOut);
        // These are network-layer failures, not anything GitHub said, so
        // none of them should namedrop the repository.
        QVERIFY(!hostNotFound.contains(QStringLiteral("bitskc")));

        const QString notFound = Lane::describeReleasesNotFound(QStringLiteral("bitskc/lane"));
        const QString unexpected = Lane::describeUnexpectedStatus(500, QStringLiteral("bitskc/lane"));
        const QString malformed = Lane::describeMalformedResponse(QStringLiteral("bitskc/lane"), QStringLiteral("the response body was not valid JSON"));
        const QString redirect = Lane::describeUnsafeRedirect(QStringLiteral("bitskc/lane"));
        QVERIFY(notFound != unexpected);
        QVERIFY(notFound != malformed);
        QVERIFY(unexpected != malformed);
        QVERIFY(redirect != notFound);
        // All four name the repository slug that was actually tried, since
        // that is exactly what makes a 404 diagnosable.
        QVERIFY(notFound.contains(QStringLiteral("bitskc/lane")));
        QVERIFY(unexpected.contains(QStringLiteral("bitskc/lane")));
        QVERIFY(malformed.contains(QStringLiteral("bitskc/lane")));
        QVERIFY(redirect.contains(QStringLiteral("bitskc/lane")));
        // The HTTP status and the detail both show up, not just a generic
        // "something went wrong".
        QVERIFY(unexpected.contains(QStringLiteral("500")));
        QVERIFY(malformed.contains(QStringLiteral("not valid JSON")));
    }

    // The 404 case is inherently ambiguous (missing repo vs. a repo with
    // no releases yet); the message must own that rather than assert one
    // cause is true.
    void releasesNotFoundIsHonestAboutAmbiguity()
    {
        const QString message = Lane::describeReleasesNotFound(QStringLiteral("bitskc/lane"));
        QVERIFY(message.contains(QStringLiteral("does not exist")));
        QVERIFY(message.contains(QStringLiteral("never published")) || message.contains(QStringLiteral("no releases")));
    }

    void rateLimitUsesResetTimeWhenKnown()
    {
        const QString withReset = Lane::describeRateLimited(4102444800LL); // 2100-01-01 UTC
        const QString withoutReset = Lane::describeRateLimited(-1);
        QVERIFY(withReset != withoutReset);
        QVERIFY(withReset.contains(QStringLiteral("after")));
        QVERIFY(!withReset.isEmpty());
        QVERIFY(!withoutReset.isEmpty());
    }
};

QTEST_MAIN(VersionTest)
#include "test_version.moc"
