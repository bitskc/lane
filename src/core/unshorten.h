#pragma once
#include <QString>

class QNetworkReply;

namespace Lane
{

QString unshortenSync(const QString &url, int timeoutMs = 2000);

// Resolve a finished HEAD reply's Location header into the candidate
// destination: 3-tier Location extraction, relative resolution, the
// FullyEncoded render, then the isSafeOpenUrl + isPrivateOrLocalHost
// gates. Returns `url` unchanged when no safe target is found. Pure of
// network state beyond the reply; tested directly in test_unshorten.
QString redirectLocationToCandidate(const QString &url, QNetworkReply *reply);

} // namespace Lane
