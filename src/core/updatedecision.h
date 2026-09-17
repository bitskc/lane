#pragma once

#include <QNetworkReply>
#include <QString>
#include <QUrl>

namespace Lane
{

// Terminal outcome of decoding a completed (non-redirect) GitHub
// releases/latest response. Deliberately mirrors only the states that
// depend on what the response actually said; UpdateChecker::Status also
// has Idle/Checking, which are about the request lifecycle, not the
// response, so they stay out of this pure decision layer.
enum class UpdateOutcome {
    UpToDate,
    UpdateAvailable,
    Failed,
};

struct UpdateDecision {
    UpdateOutcome outcome = UpdateOutcome::Failed;
    QString latestVersion;
    QString releaseUrl;
    // Only meaningful when outcome == Failed.
    QString errorMessage;
};

// Pure decode of a completed, non-redirect GitHub releases/latest response
// into a terminal update-check outcome. Takes exactly what
// UpdateChecker::handleReply() has already pulled off the QNetworkReply
// (status, error, body, and the rate-limit header), so it can be
// exercised with synthetic inputs without touching QNetworkAccessManager
// or the network. httpStatus is the resolved HTTP status (0 when no
// response was ever received at all, i.e. a pure network/DNS/TLS
// failure); networkError is reply->error(); rateLimitResetHeader is the
// raw x-ratelimit-reset header value (empty when absent); currentVersion
// is compared against the release's tag_name to decide
// UpToDate vs UpdateAvailable.
UpdateDecision decodeUpdateReply(int httpStatus,
                                  QNetworkReply::NetworkError networkError,
                                  const QByteArray &body,
                                  const QByteArray &rateLimitResetHeader,
                                  const QString &currentVersion,
                                  const QString &repoSlug);

// Whether a host is one Lane will follow or open for update checks.
// Release metadata and redirect targets must stay on GitHub itself, not
// an arbitrary https mirror that could point elsewhere.
bool isAllowedGitHubUpdateHost(const QString &host);

// Whether a 3xx redirect target is one the update checker will follow:
// valid, https, pinned to github.com or api.github.com, and not a
// private/local host.
bool isSafeUpdateRedirect(const QUrl &target);

} // namespace Lane
