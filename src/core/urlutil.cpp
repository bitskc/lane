#include "urlutil.h"

#include <optional>

#include <QHostAddress>
#include <QAbstractSocket>
#include <QRegularExpression>
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
    // One URL policy: normalizeInput decides what a click means (keeps the
    // origin, adds a scheme only to a bare host). Re-parsing the raw string
    // with fromUserInput here would re-invent the host "https" for
    // "https://example.com.." that normalizeInput deliberately left alone.
    const QUrl url(normalizeInput(raw), QUrl::TolerantMode);
    out.url = url;
    out.valid = url.isValid() && !url.host().isEmpty();
    out.scheme = url.scheme();
    out.host = url.host().toLower();
    out.path = url.path();
    out.query = url.query();
    return out;
}

// True when the string holds a character that must never reach an argv,
// a notification body or a log line as a raw character. Shared by
// normalizeInput and isSafeOpenUrl so the two cannot disagree on it.
static bool hasForbiddenControl(const QString &s)
{
    for (const QChar c : s) {
        if (c == QLatin1Char('\n') || c == QLatin1Char('\r') || c == QChar(0) || c == QChar(0x2028)
            || c == QChar(0x2029)) {
            return true;
        }
    }
    return false;
}

QString normalizeInput(const QString &raw)
{
    const QString trimmed = raw.trimmed();
    if (trimmed.isEmpty()) {
        return trimmed;
    }
    // A forbidden control character is returned untouched so that
    // isSafeOpenUrl still refuses the click. Encoding it here to %0A
    // would launder it into a string that looks safe.
    if (hasForbiddenControl(trimmed)) {
        return trimmed;
    }
    // A scheme can only sit at the very start, ahead of the first '/', '?'
    // or '#'. Looking for "://" anywhere let a bare host whose query holds
    // a URL ("example.com/login?next=https://...") skip the scheme repair
    // and then fail the strict check as a relative URL.
    const qsizetype authorityStart = trimmed.indexOf(QLatin1String("://"));
    static const QRegularExpression firstDelimiterRe(QStringLiteral("[/?#]"));
    const qsizetype firstDelimiter = trimmed.indexOf(firstDelimiterRe);
    if (authorityStart != -1 && (firstDelimiter == -1 || firstDelimiter > authorityStart)) {
        // An explicit scheme is never repaired. QUrl::fromUserInput, given
        // an authority it cannot parse ("https://example.com..",
        // "http://example.com:99999/"), guesses a scheme and moves the
        // original one into the host, producing a valid-looking URL whose
        // host is "https". TolerantMode still re-encodes a stray % or a
        // literal space, as the old code did, but keeps the origin.
        const QUrl url(trimmed, QUrl::TolerantMode);
        if (!url.isValid() || url.scheme().isEmpty() || url.host().isEmpty()) {
            return trimmed;
        }
        return url.toString(QUrl::FullyEncoded);
    }
    // No scheme: this is a bare host or host/path typed or passed by a
    // script, which is what fromUserInput is for.
    const QUrl url = QUrl::fromUserInput(trimmed);
    return url.isValid() ? url.toString(QUrl::FullyEncoded) : trimmed;
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
    // Decoded once, to undo the wrapper's own encoding. What comes out may
    // hold a literal space, so it goes back through normalizeInput before
    // the strict safety check sees it.
    QString nested = query.queryItemValue(QStringLiteral("url"), QUrl::FullyDecoded);
    if (nested.isEmpty()) {
        nested = query.queryItemValue(QStringLiteral("data"), QUrl::FullyDecoded);
    }
    const QString trimmedNested = nested.trimmed();
    if (!trimmedNested.startsWith(QLatin1String("http://"), Qt::CaseInsensitive)
        && !trimmedNested.startsWith(QLatin1String("https://"), Qt::CaseInsensitive)) {
        return raw;
    }
    nested = normalizeInput(trimmedNested);
    if (!isSafeOpenUrl(nested)) {
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

bool pathWithinPrefix(const QString &urlPath, const QString &prefixPath, Qt::CaseSensitivity cs)
{
    QString prefix = prefixPath.isEmpty() ? QStringLiteral("/") : prefixPath;
    if (prefix.size() > 1 && prefix.endsWith(QLatin1Char('/'))) {
        prefix.chop(1);
    }
    if (prefix == QLatin1String("/")) {
        return true;
    }
    QString path = urlPath.isEmpty() ? QStringLiteral("/") : urlPath;
    if (path.size() > 1 && path.endsWith(QLatin1Char('/'))) {
        path.chop(1);
    }
    if (path.compare(prefix, cs) == 0) {
        return true;
    }
    return path.startsWith(prefix + QLatin1Char('/'), cs);
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
    return pathWithinPrefix(u.path(), s.path(), Qt::CaseInsensitive);
}

// The IPv4 address an IPv6 transition prefix carries, so a redirect to
// [64:ff9b::7f00:1] is judged as the 127.0.0.1 it reaches.
//   NAT64 64:ff9b::/96 (RFC 6052): the IPv4 is the last 32 bits. Bytes 4 to
//     11 must be zero. 64:ff9b:1::/48 (RFC 8215, local use) embeds it
//     differently and is not decoded, so it is left to the range ladder.
//   6to4 2002::/16 (RFC 3056): the IPv4 is bits 16 to 47.
// Returns nothing for any other address.
static std::optional<quint32> embeddedIPv4(const Q_IPV6ADDR &a)
{
    const auto v4 = [&a](int i) {
        return (quint32(a[i]) << 24) | (quint32(a[i + 1]) << 16) | (quint32(a[i + 2]) << 8) | quint32(a[i + 3]);
    };
    if (a[0] == 0x00 && a[1] == 0x64 && a[2] == 0xff && a[3] == 0x9b) {
        for (int i = 4; i < 12; ++i) {
            if (a[i] != 0) {
                return std::nullopt;
            }
        }
        return v4(12);
    }
    if (a[0] == 0x20 && a[1] == 0x02) {
        return v4(2);
    }
    return std::nullopt;
}

bool isPrivateOrLocalHost(const QString &host)
{
    // A single trailing dot is the DNS absolute form (localhost. is
    // localhost). U+3002 and U+FF0E are the dots QUrl already folds into
    // that ASCII dot. Leaving them on made 127.0.0.1. and localhost. miss
    // every name and numeric check below.
    QString h = host.trimmed().toLower();
    h.replace(QChar(0x3002), QLatin1Char('.'));
    h.replace(QChar(0xFF0E), QLatin1Char('.'));
    while (h.endsWith(QLatin1Char('.'))) {
        h.chop(1);
    }
    if (h.isEmpty() || h == QLatin1String("localhost") || h == QLatin1String("localhost.localdomain")
        || h.endsWith(QLatin1String(".localhost")) || h.endsWith(QLatin1String(".local"))
        || h.endsWith(QLatin1String(".internal")) || h.endsWith(QLatin1String(".lan"))
        || h == QLatin1String("metadata.google.internal")) {
        return true;
    }

    QHostAddress addr(h);
    // Browsers normalize [::ffff:a.b.c.d] and integer/hex/octal IPv4
    // literals to the real address before connecting, so the guard must
    // see through the same encodings the fetch will. QHostAddress keeps
    // IPv4-mapped IPv6 as protocol IPv6, so normalize it explicitly.
    if (addr.protocol() == QAbstractSocket::IPv6Protocol) {
        bool ok = false;
        const quint32 mapped = addr.toIPv4Address(&ok);
        if (ok) {
            addr = QHostAddress(mapped);
        } else if (const auto embedded = embeddedIPv4(addr.toIPv6Address())) {
            addr = QHostAddress(*embedded);
        } else if (h.startsWith(QLatin1String("[::ffff:"))) {
            addr = QHostAddress(h.mid(8, h.size() - 9));
        }
    }
    if (addr.isNull()) {
        // Integer and hex/octal IPv4 literals (2130706433, 0x7f000001,
        // 0177.0.0.1) all parse to null here but resolve to real hosts in
        // a browser. Treat any all-numeric dotted or bare numeric host as
        // private: the worst case is a refused shortener unwrap, and
        // guessing wrong the other way leaks the redirect to LAN.
        static const QRegularExpression numericLike(
            QStringLiteral("^(?:0[xX][0-9a-fA-F]+|0[0-7]*|[0-9]+)(?:\\.(?:0[xX][0-9a-fA-F]+|0[0-7]*|[0-9]+))*$"));
        if (numericLike.match(h).hasMatch()) {
            return true;
        }
        return false;
    }
    if (addr.isLoopback() || addr.isLinkLocal() || addr.isMulticast() || addr.isBroadcast()) {
        return true;
    }
    if (addr.isSiteLocal() || addr.isUniqueLocalUnicast()) {
        return true;
    }
    if (addr.protocol() == QAbstractSocket::IPv4Protocol) {
        const quint32 ip = addr.toIPv4Address();
        const quint8 a = quint8(ip >> 24);
        const quint8 b = quint8(ip >> 16);
        const quint8 c = quint8(ip >> 8);
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
        if (a == 198 && (b == 18 || b == 19)) {
            return true;
        }
        if (a == 198 && b == 51 && c == 100) {
            return true;
        }
        // Reserved/documentation ranges that never route publicly:
        // 192.0.0.0/24 (IETF protocol assignments), 192.0.2.0/24,
        // 203.0.113.0/24, 240.0.0.0/4 (future use), 255.x handled above.
        if (a == 192 && b == 0 && c == 0) {
            return true;
        }
        if (a == 192 && b == 0 && c == 2) {
            return true;
        }
        if (a == 203 && b == 0 && c == 113) {
            return true;
        }
        if (a >= 240) {
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
    if (hasForbiddenControl(trimmed)) {
        return false;
    }
    // StrictMode never repairs. The click has already been through
    // normalizeInput, which re-encodes spaces and stray % but keeps the
    // origin, so a strict parse here only rejects what is truly malformed.
    const QUrl url(trimmed, QUrl::StrictMode);
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
    QUrl url(raw.trimmed(), QUrl::StrictMode);
    url.setUserName({});
    url.setPassword({});
    // Encoded, because the browser launch and the unshorten result both go
    // back through isSafeOpenUrl, whose strict parse refuses a literal space.
    return url.toString(QUrl::FullyEncoded);
}

QString redirectCandidate(const QUrl &location)
{
    // Encoded for the same reason as sanitizedOpenUrl. PrettyDecoded turned
    // %20 into a space, so a destination such as a SharePoint document path
    // failed the safety check and the click stayed on the shortener host.
    return location.toString(QUrl::FullyEncoded);
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
    // PrettyDecoded turns %0A, %E2%80%AE and friends back into literal
    // control/format characters. The wire side refuses raw controls, so
    // the display side must not re-inject decoded ones into a picker
    // label, a hold-bar subtitle, or the clipboard fallback.
    s.removeIf([](QChar c) {
        return (c.unicode() < 0x20) || (c.unicode() >= 0x7f && c.unicode() <= 0x9f)
            || c == QChar(0x2028) || c == QChar(0x2029)
            || (c.unicode() >= 0x202a && c.unicode() <= 0x202e)
            || (c.unicode() >= 0x2066 && c.unicode() <= 0x2069)
            || c == QChar(0x061c);
    });
    if (s.startsWith(QLatin1String("//"))) {
        s = s.mid(2);
    }
    if (s.size() > 180) {
        s = s.left(90) + QStringLiteral("…") + s.right(70);
    }
    return s;
}

} // namespace Lane
