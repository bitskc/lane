#include "updatedecision.h"

#include "updatemessages.h"
#include "urlutil.h"
#include "version.h"

#include <QJsonDocument>
#include <QJsonObject>

namespace Lane
{

bool isSafeUpdateRedirect(const QUrl &target)
{
    return target.isValid() && target.scheme() == QLatin1String("https") && !isPrivateOrLocalHost(target.host());
}

UpdateDecision decodeUpdateReply(int httpStatus,
                                  QNetworkReply::NetworkError networkError,
                                  const QByteArray &body,
                                  const QByteArray &rateLimitResetHeader,
                                  const QString &currentVersion,
                                  const QString &repoSlug)
{
    UpdateDecision result;

    if (networkError != QNetworkReply::NoError && httpStatus == 0) {
        // No HTTP response was received at all: DNS, connection, TLS, or
        // timeout failure rather than anything GitHub said.
        result.errorMessage = describeNetworkError(networkError);
        return result;
    }

    if (httpStatus == 403 || httpStatus == 429) {
        bool ok = false;
        const qint64 resetEpoch = rateLimitResetHeader.toLongLong(&ok);
        result.errorMessage = describeRateLimited(ok ? resetEpoch : -1);
        return result;
    }

    if (httpStatus == 404) {
        // Ambiguous on purpose: GitHub returns 404 both for a repository
        // that does not exist and for one with no published releases.
        result.errorMessage = describeReleasesNotFound(repoSlug);
        return result;
    }

    if (networkError != QNetworkReply::NoError || httpStatus != 200) {
        result.errorMessage = describeUnexpectedStatus(httpStatus, repoSlug);
        return result;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(body);
    if (!doc.isObject()) {
        result.errorMessage = describeMalformedResponse(repoSlug, QStringLiteral("the response body was not valid JSON"));
        return result;
    }
    const QJsonObject obj = doc.object();
    const QString tag = obj.value(QStringLiteral("tag_name")).toString();
    const QString htmlUrl = obj.value(QStringLiteral("html_url")).toString();
    if (tag.isEmpty() || htmlUrl.isEmpty()) {
        result.errorMessage =
            describeMalformedResponse(repoSlug, QStringLiteral("the release was missing its version tag or URL"));
        return result;
    }
    if (!isSafeOpenUrl(htmlUrl) || isPrivateOrLocalHost(hostOf(htmlUrl))) {
        result.errorMessage = describeMalformedResponse(repoSlug, QStringLiteral("the release URL was not safe to open"));
        return result;
    }

    result.latestVersion = tag;
    result.releaseUrl = htmlUrl;

    const VersionOrder order = compareVersions(currentVersion, tag);
    if (order == VersionOrder::Older) {
        result.outcome = UpdateOutcome::UpdateAvailable;
    } else if (order == VersionOrder::Unknown) {
        result.errorMessage =
            describeMalformedResponse(repoSlug, QStringLiteral("the version \"%1\" could not be parsed").arg(tag));
    } else {
        result.outcome = UpdateOutcome::UpToDate;
    }

    return result;
}

} // namespace Lane
