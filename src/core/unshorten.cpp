#include "unshorten.h"

#include <QEventLoop>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>

namespace Tern
{

QString unshortenSync(const QString &url, int timeoutMs)
{
    QNetworkAccessManager nam;
    QNetworkRequest req{QUrl(url)};
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    req.setMaximumRedirectsAllowed(0);
    req.setTransferTimeout(timeoutMs);

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
        const QUrl loc = reply->header(QNetworkRequest::LocationHeader).toUrl();
        if (loc.isValid()) {
            result = loc.toString();
        } else {
            const QByteArray raw = reply->rawHeader("Location");
            if (!raw.isEmpty()) {
                result = QString::fromUtf8(raw);
            }
        }
    }
    reply->deleteLater();
    return result;
}

} // namespace Tern
