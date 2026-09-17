#include "UpdateChecker.h"

#include "core/repoinfo.h"
#include "core/updatedecision.h"
#include "core/updatemessages.h"
#include "lane_version.h"

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
    if (!isSafeUpdateRedirect(url)) {
        fail(describeUnsafeRedirect(QLatin1String(kGitHubRepoSlug)));
        return;
    }

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
    // decision logic below would otherwise mistake an ordinary redirect
    // response for a connection failure.
    if (httpStatus >= 300 && httpStatus < 400) {
        QUrl location = reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
        if (location.isRelative()) {
            location = reply->url().resolved(location);
        }
        if (!isSafeUpdateRedirect(location) || redirectsLeft <= 0) {
            fail(describeUnsafeRedirect(repoSlug));
            return;
        }
        startRequest(location, redirectsLeft - 1);
        return;
    }

    // Everything below is pure decode of the finished, non-redirect reply
    // into a terminal state; see core/updatedecision.h for the parts that
    // are unit tested directly without a QNetworkReply.
    const UpdateDecision decision = decodeUpdateReply(httpStatus,
                                                        reply->error(),
                                                        reply->readAll(),
                                                        reply->rawHeader("x-ratelimit-reset"),
                                                        m_currentVersion,
                                                        repoSlug);
    m_latestVersion = decision.latestVersion;
    m_releaseUrl = decision.releaseUrl;
    if (decision.outcome == UpdateOutcome::Failed) {
        fail(decision.errorMessage);
        return;
    }
    setStatus(decision.outcome == UpdateOutcome::UpdateAvailable ? Status::UpdateAvailable : Status::UpToDate);
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
