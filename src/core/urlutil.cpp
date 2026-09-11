#include "urlutil.h"

#include <QHostAddress>
#include <QAbstractSocket>
#include <QSet>
#include <QUrlQuery>

namespace Lane
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
    QString nested = query.queryItemValue(QStringLiteral("url"), QUrl::FullyDecoded);
    if (nested.isEmpty()) {
        nested = query.queryItemValue(QStringLiteral("data"), QUrl::FullyDecoded);
    }
    if (nested.isEmpty() || !isSafeOpenUrl(nested)) {
        return raw;
    }
    return nested;
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

bool isPrivateOrLocalHost(const QString &host)
{
    const QString h = host.trimmed().toLower();
    if (h.isEmpty() || h == QLatin1String("localhost") || h == QLatin1String("localhost.localdomain")
        || h.endsWith(QLatin1String(".localhost")) || h.endsWith(QLatin1String(".local"))
        || h.endsWith(QLatin1String(".internal")) || h.endsWith(QLatin1String(".lan"))
        || h == QLatin1String("metadata.google.internal")) {
        return true;
    }

    const QHostAddress addr(h);
    if (addr.isNull()) {
        return false;
    }
    if (addr.isLoopback() || addr.isLinkLocal() || addr.isMulticast() || addr.isBroadcast()) {
        return true;
    }
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
    if (addr.isSiteLocal() || addr.isUniqueLocalUnicast()) {
        return true;
    }
#endif
    if (addr.protocol() == QAbstractSocket::IPv4Protocol) {
        const quint32 ip = addr.toIPv4Address();
        const quint8 a = quint8(ip >> 24);
        const quint8 b = quint8(ip >> 16);
        if (a == 10 || a == 127 || a == 0) {
            return true;
        }
        if (a == 100 && b >= 64 && b <= 127) {
            return true;
        }
        if (a == 169 && b == 254) {
            return true;
        }
        if (a == 172 && b >= 16 && b <= 31) {
            return true;
        }
        if (a == 192 && b == 168) {
            return true;
        }
        if (a == 198 && (b == 18 || b == 51)) {
            return true;
        }
    }
    return false;
}

bool isSafeOpenUrl(const QString &raw)
{
    const QString trimmed = raw.trimmed();
    if (trimmed.isEmpty() || trimmed.size() > 8192) {
        return false;
    }
    if (trimmed.contains(QLatin1Char('\n')) || trimmed.contains(QLatin1Char('\r'))
        || trimmed.contains(QChar(0))) {
        return false;
    }
    QUrl url = QUrl::fromUserInput(trimmed);
    if (!url.isValid() || url.isRelative() || url.host().isEmpty()) {
        return false;
    }
    const QString scheme = url.scheme().toLower();
    if (scheme != QLatin1String("http") && scheme != QLatin1String("https")) {
        return false;
    }
    if (!url.userName().isEmpty() || !url.password().isEmpty()) {
        return false;
    }
    return true;
}

QString sanitizedOpenUrl(const QString &raw)
{
    if (!isSafeOpenUrl(raw)) {
        return {};
    }
    QUrl url = QUrl::fromUserInput(raw.trimmed());
    url.setUserName({});
    url.setPassword({});
    return url.toString();
}

QString displayUrl(const QString &raw)
{
    QUrl url = QUrl::fromUserInput(raw.trimmed());
    if (!url.isValid()) {
        return raw.left(180);
    }
    url.setUserName({});
    url.setPassword({});
    QString s = url.toString(QUrl::PrettyDecoded | QUrl::RemoveScheme | QUrl::RemoveUserInfo);
    if (s.startsWith(QLatin1String("//"))) {
        s = s.mid(2);
    }
    if (s.size() > 180) {
        s = s.left(90) + QStringLiteral("…") + s.right(70);
    }
    return s;
}

} // namespace Lane
