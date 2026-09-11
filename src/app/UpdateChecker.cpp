#include "UpdateChecker.h"

#include "core/repoinfo.h"
#include "core/updatemessages.h"
#include "core/urlutil.h"
#include "core/version.h"
#include "lane_version.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>

namespace Lane
{

namespace
{
constexpr int kRequestTimeoutMs = 8000;
constexpr int kMaxRedirectHops = 3;

QByteArray userAgent()
{
    return QByteArrayLiteral("Lane/") + QByteArray(LANE_VERSION_STRING) + QByteArrayLiteral(" (+")
        + githubProjectUrl().toUtf8() + QByteArrayLiteral(")");
}

} // namespace

UpdateChecker::UpdateChecker(QString currentVersion, QObject *parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
    , m_currentVersion(std::move(currentVersion))
{
    // Plain GET only: no cookies persisted across requests.
    m_nam->setCookieJar(nullptr);
}

void UpdateChecker::check()
{
    if (m_status == Status::Checking) {
        return;
    }
    m_latestVersion.clear();
    m_releaseUrl.clear();
    m_errorMessage.clear();
    setStatus(Status::Checking);
    startRequest(QUrl(githubLatestReleaseApiUrl()), kMaxRedirectHops);
}

void UpdateChecker::startRequest(const QUrl &url, int redirectsLeft)
{
    QNetworkRequest req(url);
    // We resolve and vet redirects ourselves below rather than letting Qt
    // follow them, so a redirect to a non-https or private address is never
    // taken.
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    req.setAttribute(QNetworkRequest::CookieSaveControlAttribute, QNetworkRequest::Manual);
    req.setTransferTimeout(kRequestTimeoutMs);
    req.setRawHeader("User-Agent", userAgent());
    req.setRawHeader("Accept", "application/vnd.github+json");

    QNetworkReply *reply = m_nam->get(req);
    m_reply = reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply, redirectsLeft]() {
        handleReply(reply, redirectsLeft);
    });
}

void UpdateChecker::handleReply(QNetworkReply *reply, int redirectsLeft)
{
    reply->deleteLater();
    if (reply != m_reply) {
        // Superseded by a later check() call; drop this stale reply.
        return;
    }
    m_reply.clear();
    m_lastCheckedAt = QDateTime::currentDateTime();

    const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QString repoSlug = QLatin1String(kGitHubRepoSlug);

    // A redirect arrives as a 3xx status with QNetworkReply::NoError (manual
    // redirect policy leaves error() untouched); handle it before the
    // network-error check below would otherwise mistake an ordinary
    // redirect response for a connection failure.
    if (httpStatus >= 300 && httpStatus < 400) {
        QUrl location = reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
        if (location.isRelative()) {
            location = reply->url().resolved(location);
        }
        const bool safe = location.isValid() && location.scheme() == QLatin1String("https")
            && !isPrivateOrLocalHost(location.host());
        if (!safe || redirectsLeft <= 0) {
            fail(describeUnsafeRedirect(repoSlug));
            return;
        }
        startRequest(location, redirectsLeft - 1);
        return;
    }

    if (reply->error() != QNetworkReply::NoError && httpStatus == 0) {
        // No HTTP response was received at all: DNS, connection, TLS, or
        // timeout failure rather than anything GitHub said.
        fail(describeNetworkError(reply->error()));
        return;
    }

    if (httpStatus == 403 || httpStatus == 429) {
        const QByteArray resetHeader = reply->rawHeader("x-ratelimit-reset");
        bool ok = false;
        const qint64 resetEpoch = resetHeader.toLongLong(&ok);
        fail(describeRateLimited(ok ? resetEpoch : -1));
        return;
    }

    if (httpStatus == 404) {
        // Ambiguous on purpose: GitHub returns 404 both for a repository
        // that does not exist and for one with no published releases.
        fail(describeReleasesNotFound(repoSlug));
        return;
    }

    if (reply->error() != QNetworkReply::NoError || httpStatus != 200) {
        fail(describeUnexpectedStatus(httpStatus, repoSlug));
        return;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    if (!doc.isObject()) {
        fail(describeMalformedResponse(repoSlug, QStringLiteral("the response body was not valid JSON")));
        return;
    }
    const QJsonObject obj = doc.object();
    const QString tag = obj.value(QStringLiteral("tag_name")).toString();
    const QString htmlUrl = obj.value(QStringLiteral("html_url")).toString();
    if (tag.isEmpty() || htmlUrl.isEmpty()) {
        fail(describeMalformedResponse(repoSlug, QStringLiteral("the release was missing its version tag or URL")));
        return;
    }
    if (!isSafeOpenUrl(htmlUrl) || isPrivateOrLocalHost(hostOf(htmlUrl))) {
        fail(describeMalformedResponse(repoSlug, QStringLiteral("the release URL was not safe to open")));
        return;
    }

    m_latestVersion = tag;
    m_releaseUrl = htmlUrl;

    const VersionOrder order = compareVersions(m_currentVersion, tag);
    if (order == VersionOrder::Older) {
        setStatus(Status::UpdateAvailable);
    } else if (order == VersionOrder::Unknown) {
        fail(describeMalformedResponse(repoSlug, QStringLiteral("the version \"%1\" could not be parsed").arg(tag)));
    } else {
        setStatus(Status::UpToDate);
    }
}

void UpdateChecker::setStatus(Status status)
{
    m_status = status;
    Q_EMIT statusChanged();
}

void UpdateChecker::fail(const QString &message)
{
    m_errorMessage = message;
    setStatus(Status::Failed);
}

} // namespace Lane
