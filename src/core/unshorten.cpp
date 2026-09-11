#include "unshorten.h"

#include "urlutil.h"

#include <QEventLoop>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>

namespace Lane
{

QString unshortenSync(const QString &url, int timeoutMs)
{
    if (!isShortener(url) || !isSafeOpenUrl(url)) {
        return url;
    }

    QNetworkAccessManager nam;
    nam.setRedirectPolicy(QNetworkRequest::ManualRedirectPolicy);
    QNetworkRequest req{QUrl(url)};
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    req.setAttribute(QNetworkRequest::CookieSaveControlAttribute, QNetworkRequest::Manual);
    req.setTransferTimeout(timeoutMs);
    req.setRawHeader("User-Agent", "Lane/0.1");
    req.setMaximumRedirectsAllowed(0);

    QNetworkReply *reply = nam.head(req);
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    timer.start(timeoutMs);
    loop.exec();

    QString result = url;
    if (reply->isFinished()) {
        QUrl loc = reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
        if (!loc.isValid()) {
            loc = reply->header(QNetworkRequest::LocationHeader).toUrl();
        }
        if (!loc.isValid()) {
            const QByteArray raw = reply->rawHeader("Location");
            if (!raw.isEmpty() && !raw.contains('\0') && raw.size() < 4096) {
                loc = QUrl::fromEncoded(raw);
            }
        }
        if (loc.isValid()) {
            if (loc.isRelative()) {
                loc = QUrl(url).resolved(loc);
            }
            const QString candidate = loc.toString();
            if (isSafeOpenUrl(candidate) && !isPrivateOrLocalHost(hostOf(candidate))) {
                result = sanitizedOpenUrl(candidate);
            }
        }
    }
    reply->deleteLater();
    return result.isEmpty() ? url : result;
}

} // namespace Lane
