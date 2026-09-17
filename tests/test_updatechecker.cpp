#include "core/updatedecision.h"

#include <QTest>

using namespace Lane;

namespace
{
QByteArray releaseBody(const QString &tag, const QString &htmlUrl)
{
    return QByteArrayLiteral("{\"tag_name\":\"") + tag.toUtf8() + QByteArrayLiteral("\",\"html_url\":\"")
        + htmlUrl.toUtf8() + QByteArrayLiteral("\"}");
}
} // namespace

class UpdateCheckerTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void validReplyNewerRemoteIsUpdateAvailable()
    {
        const QByteArray body = releaseBody(QStringLiteral("v0.3.0"), QStringLiteral("https://github.com/bitskc/lane/releases/tag/v0.3.0"));
        const UpdateDecision d = decodeUpdateReply(200, QNetworkReply::NoError, body, QByteArray(),
                                                    QStringLiteral("0.2.0"), QStringLiteral("bitskc/lane"));
        QCOMPARE(static_cast<int>(d.outcome), static_cast<int>(UpdateOutcome::UpdateAvailable));
        QCOMPARE(d.latestVersion, QStringLiteral("v0.3.0"));
        QCOMPARE(d.releaseUrl, QStringLiteral("https://github.com/bitskc/lane/releases/tag/v0.3.0"));
        QVERIFY(d.errorMessage.isEmpty());
    }

    void validReplySameOrOlderRemoteIsUpToDate()
    {
        const QByteArray body = releaseBody(QStringLiteral("v0.2.0"), QStringLiteral("https://github.com/bitskc/lane/releases/tag/v0.2.0"));
        const UpdateDecision same = decodeUpdateReply(200, QNetworkReply::NoError, body, QByteArray(),
                                                        QStringLiteral("0.2.0"), QStringLiteral("bitskc/lane"));
        QCOMPARE(static_cast<int>(same.outcome), static_cast<int>(UpdateOutcome::UpToDate));

        const QByteArray olderBody = releaseBody(QStringLiteral("v0.1.0"), QStringLiteral("https://github.com/bitskc/lane/releases/tag/v0.1.0"));
        const UpdateDecision older = decodeUpdateReply(200, QNetworkReply::NoError, olderBody, QByteArray(),
                                                         QStringLiteral("0.2.0"), QStringLiteral("bitskc/lane"));
        QCOMPARE(static_cast<int>(older.outcome), static_cast<int>(UpdateOutcome::UpToDate));
    }

    void notFoundStatusFails()
    {
        const UpdateDecision d = decodeUpdateReply(404, QNetworkReply::NoError, QByteArray(), QByteArray(),
                                                    QStringLiteral("0.2.0"), QStringLiteral("bitskc/lane"));
        QCOMPARE(static_cast<int>(d.outcome), static_cast<int>(UpdateOutcome::Failed));
        QVERIFY(d.errorMessage.contains(QStringLiteral("bitskc/lane")));
        QVERIFY(d.latestVersion.isEmpty());
    }

    void pureNetworkErrorFails()
    {
        // httpStatus == 0 alongside a non-NoError is a connection failure,
        // never something GitHub actually said; must never be mistaken for
        // a 200-with-garbage-body case.
        const UpdateDecision d = decodeUpdateReply(0, QNetworkReply::HostNotFoundError, QByteArray(), QByteArray(),
                                                    QStringLiteral("0.2.0"), QStringLiteral("bitskc/lane"));
        QCOMPARE(static_cast<int>(d.outcome), static_cast<int>(UpdateOutcome::Failed));
        QVERIFY(!d.errorMessage.contains(QStringLiteral("bitskc/lane")));
    }

    void rateLimitedUsesResetHeaderWhenPresent()
    {
        const UpdateDecision withReset = decodeUpdateReply(403, QNetworkReply::NoError, QByteArray(),
                                                             QByteArrayLiteral("4102444800"), // 2100-01-01 UTC
                                                             QStringLiteral("0.2.0"), QStringLiteral("bitskc/lane"));
        const UpdateDecision withoutReset = decodeUpdateReply(429, QNetworkReply::NoError, QByteArray(), QByteArray(),
                                                                QStringLiteral("0.2.0"), QStringLiteral("bitskc/lane"));
        const UpdateDecision forbidden403 = decodeUpdateReply(403, QNetworkReply::NoError, QByteArray(), QByteArray(),
                                                               QStringLiteral("0.2.0"), QStringLiteral("bitskc/lane"));
        QCOMPARE(static_cast<int>(withReset.outcome), static_cast<int>(UpdateOutcome::Failed));
        QCOMPARE(static_cast<int>(withoutReset.outcome), static_cast<int>(UpdateOutcome::Failed));
        QCOMPARE(static_cast<int>(forbidden403.outcome), static_cast<int>(UpdateOutcome::Failed));
        QVERIFY(withReset.errorMessage.contains(QStringLiteral("rate limiting")));
        QVERIFY(withoutReset.errorMessage.contains(QStringLiteral("rate limiting")));
        QVERIFY(!forbidden403.errorMessage.contains(QStringLiteral("rate limiting")));
        QVERIFY(forbidden403.errorMessage.contains(QStringLiteral("HTTP 403")));
    }

    void malformedJsonFails()
    {
        const UpdateDecision d = decodeUpdateReply(200, QNetworkReply::NoError, QByteArrayLiteral("not json"),
                                                    QByteArray(), QStringLiteral("0.2.0"), QStringLiteral("bitskc/lane"));
        QCOMPARE(static_cast<int>(d.outcome), static_cast<int>(UpdateOutcome::Failed));
        QVERIFY(d.errorMessage.contains(QStringLiteral("valid JSON")));
    }

    void missingFieldsFail()
    {
        const UpdateDecision d = decodeUpdateReply(200, QNetworkReply::NoError, QByteArrayLiteral("{\"tag_name\":\"v1.0.0\"}"),
                                                    QByteArray(), QStringLiteral("0.2.0"), QStringLiteral("bitskc/lane"));
        QCOMPARE(static_cast<int>(d.outcome), static_cast<int>(UpdateOutcome::Failed));
    }

    void unsafeReleaseUrlFails()
    {
        const QByteArray body = releaseBody(QStringLiteral("v1.0.0"), QStringLiteral("http://127.0.0.1/x"));
        const UpdateDecision d = decodeUpdateReply(200, QNetworkReply::NoError, body, QByteArray(),
                                                    QStringLiteral("0.2.0"), QStringLiteral("bitskc/lane"));
        QCOMPARE(static_cast<int>(d.outcome), static_cast<int>(UpdateOutcome::Failed));

        const QByteArray offHostBody =
            releaseBody(QStringLiteral("v1.0.0"), QStringLiteral("https://evil.example/releases/tag/v1.0.0"));
        const UpdateDecision offHost = decodeUpdateReply(200, QNetworkReply::NoError, offHostBody, QByteArray(),
                                                          QStringLiteral("0.2.0"), QStringLiteral("bitskc/lane"));
        QCOMPARE(static_cast<int>(offHost.outcome), static_cast<int>(UpdateOutcome::Failed));
    }

    void unparsableVersionFailsButKeepsReleaseInfo()
    {
        const QByteArray body = releaseBody(QStringLiteral("not-a-version"), QStringLiteral("https://github.com/bitskc/lane/releases/tag/x"));
        const UpdateDecision d = decodeUpdateReply(200, QNetworkReply::NoError, body, QByteArray(),
                                                    QStringLiteral("0.2.0"), QStringLiteral("bitskc/lane"));
        QCOMPARE(static_cast<int>(d.outcome), static_cast<int>(UpdateOutcome::Failed));
        // The release info that was successfully parsed stays available
        // even though the version comparison itself could not complete.
        QCOMPARE(d.latestVersion, QStringLiteral("not-a-version"));
    }

    void redirectSafetyRejectsNonHttpsAndPrivateHosts()
    {
        QVERIFY(isSafeUpdateRedirect(QUrl(QStringLiteral("https://api.github.com/repos/bitskc/lane/releases/latest"))));
        QVERIFY(isSafeUpdateRedirect(QUrl(QStringLiteral("https://github.com/bitskc/lane/releases/latest"))));
        QVERIFY(!isSafeUpdateRedirect(QUrl(QStringLiteral("http://api.github.com/repos/bitskc/lane/releases/latest"))));
        QVERIFY(!isSafeUpdateRedirect(QUrl(QStringLiteral("https://127.0.0.1/evil"))));
        QVERIFY(!isSafeUpdateRedirect(QUrl(QStringLiteral("https://localhost/evil"))));
        QVERIFY(!isSafeUpdateRedirect(QUrl(QStringLiteral("https://evil.example/redirect"))));
        QVERIFY(!isSafeUpdateRedirect(QUrl()));
    }

    void githubHostPinningAllowsOnlyGitHubHosts()
    {
        QVERIFY(isAllowedGitHubUpdateHost(QStringLiteral("github.com")));
        QVERIFY(isAllowedGitHubUpdateHost(QStringLiteral("api.github.com")));
        QVERIFY(isAllowedGitHubUpdateHost(QStringLiteral("GitHub.COM")));
        QVERIFY(!isAllowedGitHubUpdateHost(QStringLiteral("raw.githubusercontent.com")));
        QVERIFY(!isAllowedGitHubUpdateHost(QStringLiteral("evil.github.com")));
    }
};

QTEST_MAIN(UpdateCheckerTest)
#include "test_updatechecker.moc"
