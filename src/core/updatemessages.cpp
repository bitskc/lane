#include "updatemessages.h"

#include <QDateTime>
#include <QLocale>

namespace Lane
{

QString describeNetworkError(QNetworkReply::NetworkError error)
{
    switch (error) {
    case QNetworkReply::HostNotFoundError:
        return QStringLiteral("Could not resolve GitHub's address. Check your DNS or network connection.");
    case QNetworkReply::ConnectionRefusedError:
        return QStringLiteral("The connection to GitHub was refused. Check your network connection.");
    case QNetworkReply::RemoteHostClosedError:
        return QStringLiteral("GitHub closed the connection unexpectedly. Try again.");
    case QNetworkReply::TimeoutError:
        return QStringLiteral("The request to GitHub timed out. Check your network connection and try again.");
    case QNetworkReply::SslHandshakeFailedError:
        return QStringLiteral("A secure connection to GitHub could not be established.");
    case QNetworkReply::TemporaryNetworkFailureError:
    case QNetworkReply::NetworkSessionFailedError:
        return QStringLiteral("No network connection is available. Connect to the network and try again.");
    case QNetworkReply::ProxyConnectionRefusedError:
    case QNetworkReply::ProxyConnectionClosedError:
    case QNetworkReply::ProxyNotFoundError:
    case QNetworkReply::ProxyTimeoutError:
    case QNetworkReply::ProxyAuthenticationRequiredError:
        return QStringLiteral("A proxy between Lane and GitHub refused or failed the connection.");
    default:
        return QStringLiteral("Could not reach GitHub. Check your network connection and try again.");
    }
}

QString describeRateLimited(qint64 resetEpochSeconds)
{
    if (resetEpochSeconds > 0) {
        const QDateTime resetAt = QDateTime::fromSecsSinceEpoch(resetEpochSeconds);
        return QStringLiteral("GitHub is rate limiting requests right now. Try again after %1.")
            .arg(QLocale::system().toString(resetAt.toLocalTime(), QLocale::ShortFormat));
    }
    return QStringLiteral("GitHub is rate limiting requests right now. Try again later.");
}

QString describeReleasesNotFound(const QString &repoSlug)
{
    return QStringLiteral(
               "GitHub found no releases at %1. Either the repository does not exist there, or it exists but has "
               "never published a release; a plain 404 does not say which.")
        .arg(repoSlug);
}

QString describeUnexpectedStatus(int httpStatus, const QString &repoSlug)
{
    return QStringLiteral("GitHub returned an unexpected response (HTTP %1) for %2. Try again later.")
        .arg(httpStatus)
        .arg(repoSlug);
}

QString describeUnsafeRedirect(const QString &repoSlug)
{
    return QStringLiteral("The update check for %1 was redirected somewhere Lane will not follow. Try again later.")
        .arg(repoSlug);
}

QString describeMalformedResponse(const QString &repoSlug, const QString &detail)
{
    return QStringLiteral("GitHub's response for %1 was not what Lane expected (%2). This looks like a bug worth reporting.")
        .arg(repoSlug, detail);
}

} // namespace Lane
