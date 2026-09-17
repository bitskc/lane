#pragma once

#include <QNetworkReply>
#include <QString>

namespace Lane
{

// Pure, testable mappings from "what happened while checking GitHub for a
// new release" to a message a user can act on. UpdateChecker::handleReply()
// picks which of these to call; kept separate from UpdateChecker itself
// (which is not part of lane-core) so the message text can be unit tested
// without touching QNetworkAccessManager or the network.

// No HTTP response was ever received: DNS failure, refused connection,
// timeout, TLS failure, and similar. Distinguishes the common cases so
// "you are offline" reads differently from "GitHub is down".
QString describeNetworkError(QNetworkReply::NetworkError error);

// GitHub answered 429, or 403 with a parsed x-ratelimit-reset header
// (rate limited). Other 403 responses are reported as unexpected HTTP
// status, not as rate limiting.
QString describeRateLimited(qint64 resetEpochSeconds);

// GitHub answered 404 for releases/latest. This is genuinely ambiguous:
// it fires both when the repository does not exist and when it exists
// but has never published a release. Rather than guess which, the
// message says both are possible and names the slug that was tried so
// the reader can tell for themselves.
QString describeReleasesNotFound(const QString &repoSlug);

// Any other non-2xx/3xx HTTP status not covered above (500s, odd codes),
// or a network-layer error that nonetheless carried a status code.
QString describeUnexpectedStatus(int httpStatus, const QString &repoSlug);

// The redirect chain following releases/latest was unsafe (non-https, a
// private/local host) or ran out of hops to follow.
QString describeUnsafeRedirect(const QString &repoSlug);

// The response was HTTP 200 but the body was not usable: not JSON,
// missing tag_name/html_url, an unsafe html_url, or a tag_name that does
// not parse as a version. detail names which of those happened so this
// reads as a distinct, bug-report-worthy case rather than a network
// problem.
QString describeMalformedResponse(const QString &repoSlug, const QString &detail);

} // namespace Lane
