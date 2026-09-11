#pragma once

#include <QString>
#include <QUrl>

namespace Tern
{

struct ParsedUrl {
    QString scheme;
    QString host;
    QString path;
    QString query;
    QUrl url;
    bool valid = false;
};

ParsedUrl parseUrl(const QString &raw);
QString normalizeInput(const QString &raw);
QString hostOf(const QString &raw);
QString pathOf(const QString &raw);
QString unwrapO365(const QString &raw);
bool isO365Wrapper(const QString &raw);
bool isShortener(const QString &raw);
QStringList shortenerHosts();
bool urlInScope(const QString &url, const QString &scope);
bool isSafeOpenUrl(const QString &raw);
bool isPrivateOrLocalHost(const QString &host);
QString displayUrl(const QString &raw);
QString sanitizedOpenUrl(const QString &raw);

} // namespace Tern
