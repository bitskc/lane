#pragma once

#include "types.h"

#include <functional>

namespace Lane
{

using UnshortenFn = std::function<QString(const QString &)>;

// Raw-segment tracking-parameter cleaner. Never routes the query through
// QUrl/QUrlQuery: that re-serializes '=' inside values (corrupting base64
// padding like "YWJj=="), re-encodes IRI paths, and punycodes IDN hosts
// the moment any parameter is touched. Instead this slices the raw query
// text between '?' and '#', drops banned segments, and splices the
// survivors back byte-for-byte. Non-http(s) schemes (mailto:, file:, ...)
// are returned untouched. `removedKeys`, if given, is cleared and filled
// with the raw (pre-decode) key text of every segment that was dropped.
QString stripTrackingParams(const QString &url, QStringList *removedKeys = nullptr);

Click runPipeline(const QString &rawUrl, const Config &config, const UnshortenFn &unshorten = {});

} // namespace Lane
