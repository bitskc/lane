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
// The openable URL a clipboard/pasteboard string represents: the string
// itself when it already passes isSafeOpenUrl, or https:// + string when
// it is an unmistakable bare host[/path] (dotted hostname). Anything else
// returns empty. Shared by the tray/status-item action and --clipboard.
QString clipboardUrlCandidate(const QString &raw);
bool isPrivateOrLocalHost(const QString &host);
QString displayUrl(const QString &raw);
QString sanitizedOpenUrl(const QString &raw);
// The string form of a redirect Location, for the safety check and the
// click. Lives here so the unshortener and the tests share one rendering.
QString redirectCandidate(const QUrl &location);
// Extracts the real URL out of a registered lane://open?url=<encoded>
// wrapper (opaque "lane:open?url=..." also accepted). Returns empty for
// a non-lane string, an unrecognized lane path, or an inner URL that
// fails isSafeOpenUrl — so file:/javascript:/nested wrappers never reach
// the pipeline.
QString unwrapLaneUrl(const QString &raw);

} // namespace Lane
