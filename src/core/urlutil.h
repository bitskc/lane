#pragma once

#include <QString>
#include <QUrl>

namespace Lane
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
// Segment-aware prefix match: urlPath matches prefixPath exactly or is a
// descendant of it on a '/' boundary (so prefix "/bits" does not match
// "/bitskc/lane"). Both paths are trailing-slash normalized. Shared by
// urlutil's urlInScope (case-insensitive: PWA scope matching is meant to be
// forgiving) and destination.cpp's destinationKeyMatches (case-sensitive:
// remembered-destination keys track URL path case, which servers treat as
// significant) — callers intentionally pass different sensitivities.
bool pathWithinPrefix(const QString &urlPath, const QString &prefixPath,
                       Qt::CaseSensitivity cs = Qt::CaseSensitive);
bool isSafeOpenUrl(const QString &raw);
bool isPrivateOrLocalHost(const QString &host);
QString displayUrl(const QString &raw);
QString sanitizedOpenUrl(const QString &raw);

} // namespace Lane
