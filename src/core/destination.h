#pragma once

#include "types.h"

namespace Tern
{

// host, or host/path, no scheme or query. github.com/bitskc
QStringList destinationLadder(const QString &url);
QString destinationKeyMatchesBest(const QString &url, const QMap<QString, QString> &remembered);
bool destinationKeyMatches(const QString &key, const QString &url);
bool isAppChromeSegment(const QString &segment);
int suggestedLadderIndex(const QString &url, const Target *target, const QMap<QString, QString> &remembered);
bool pwaShouldAutoOpen(const Target &pwa, const QString &url);
QString lookupRemembered(const QString &url, const QMap<QString, QString> &remembered);

} // namespace Tern
