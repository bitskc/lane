#include "urlutil.h"

#include <QSet>
#include <QUrlQuery>

namespace Tern
{

static const QSet<QString> kShorteners = {
    QStringLiteral("adf.ly"),
    QStringLiteral("adfoc.us"),
    QStringLiteral("bc.vc"),
    QStringLiteral("bit.ly"),
    QStringLiteral("bl.ink"),
    QStringLiteral("geni.us"),
    QStringLiteral("gg.gg"),
    QStringLiteral("linkjoy.io"),
    QStringLiteral("linktr.ee"),
    QStringLiteral("ow.ly"),
    QStringLiteral("ouo.io"),
    QStringLiteral("pxlme.me"),
    QStringLiteral("rb.gy"),
    QStringLiteral("rebrand.ly"),
    QStringLiteral("short.io"),
    QStringLiteral("shorte.st"),
    QStringLiteral("shorturl.at"),
    QStringLiteral("snip.ly"),
    QStringLiteral("t.co"),
    QStringLiteral("t2m.io"),
    QStringLiteral("tiny.one"),
    QStringLiteral("tinyurl.com"),
    QStringLiteral("vrch.at"),
};

ParsedUrl parseUrl(const QString &raw)
{
    ParsedUrl out;
    const QUrl url = QUrl::fromUserInput(raw.trimmed());
    out.url = url;
    out.valid = url.isValid() && !url.host().isEmpty();
    out.scheme = url.scheme();
    out.host = url.host().toLower();
    out.path = url.path();
    out.query = url.query();
    return out;
}

QString normalizeInput(const QString &raw)
{
    const QString trimmed = raw.trimmed();
    if (trimmed.isEmpty()) {
        return trimmed;
    }
    const QUrl url = QUrl::fromUserInput(trimmed);
    if (url.isValid()) {
        return url.toString();
    }
    return trimmed;
}

QString hostOf(const QString &raw)
{
    return parseUrl(raw).host;
}

QString pathOf(const QString &raw)
{
    const auto p = parseUrl(raw);
    if (p.query.isEmpty()) {
        return p.path;
    }
    return p.path + QLatin1Char('?') + p.query;
}

bool isO365Wrapper(const QString &raw)
{
    const QString host = hostOf(raw);
    return host.endsWith(QLatin1String(".safelinks.protection.outlook.com"))
        || host == QLatin1String("statics.teams.cdn.office.net")
        || host == QLatin1String("teams.public.onecdn.static.microsoft");
}

QString unwrapO365(const QString &raw)
{
    if (!isO365Wrapper(raw)) {
        return raw;
    }
    const QUrl url = QUrl::fromUserInput(raw);
    const QUrlQuery query(url);
    const QString nested = query.queryItemValue(QStringLiteral("url"), QUrl::FullyDecoded);
    if (!nested.isEmpty()) {
        return nested;
    }
    const QString data = query.queryItemValue(QStringLiteral("data"), QUrl::FullyDecoded);
    if (!data.isEmpty()) {
        return data;
    }
    return raw;
}

bool isShortener(const QString &raw)
{
    return kShorteners.contains(hostOf(raw));
}

QStringList shortenerHosts()
{
    return QStringList(kShorteners.begin(), kShorteners.end());
}

bool urlInScope(const QString &url, const QString &scope)
{
    if (scope.isEmpty()) {
        return false;
    }
    const QUrl u = QUrl::fromUserInput(url);
    const QUrl s = QUrl::fromUserInput(scope);
    if (!u.isValid() || !s.isValid()) {
        return url.startsWith(scope, Qt::CaseInsensitive);
    }
    if (u.scheme() != s.scheme()) {
        return false;
    }
    if (u.host().compare(s.host(), Qt::CaseInsensitive) != 0) {
        return false;
    }
    const QString sp = s.path().isEmpty() ? QStringLiteral("/") : s.path();
    const QString up = u.path().isEmpty() ? QStringLiteral("/") : u.path();
    if (sp == QLatin1String("/")) {
        return true;
    }
    return up.startsWith(sp, Qt::CaseInsensitive);
}

} // namespace Tern
