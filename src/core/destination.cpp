#include "destination.h"

#include "urlutil.h"

#include <QSet>

namespace Tern
{
namespace
{

const QSet<QString> &chromeSegments()
{
    static const QSet<QString> k = {
        QStringLiteral("about"),
        QStringLiteral("account"),
        QStringLiteral("accounts"),
        QStringLiteral("api"),
        QStringLiteral("app"),
        QStringLiteral("apps"),
        QStringLiteral("asset"),
        QStringLiteral("assets"),
        QStringLiteral("auth"),
        QStringLiteral("authorize"),
        QStringLiteral("blog"),
        QStringLiteral("c"),
        QStringLiteral("calendar"),
        QStringLiteral("callback"),
        QStringLiteral("chat"),
        QStringLiteral("compose"),
        QStringLiteral("customer"),
        QStringLiteral("customers"),
        QStringLiteral("dashboard"),
        QStringLiteral("device"),
        QStringLiteral("devices"),
        QStringLiteral("docs"),
        QStringLiteral("documents"),
        QStringLiteral("download"),
        QStringLiteral("downloads"),
        QStringLiteral("drive"),
        QStringLiteral("embed"),
        QStringLiteral("enterprise"),
        QStringLiteral("explore"),
        QStringLiteral("features"),
        QStringLiteral("graphql"),
        QStringLiteral("help"),
        QStringLiteral("home"),
        QStringLiteral("homepage"),
        QStringLiteral("inbox"),
        QStringLiteral("invoice"),
        QStringLiteral("invoices"),
        QStringLiteral("issues"),
        QStringLiteral("legal"),
        QStringLiteral("login"),
        QStringLiteral("logout"),
        QStringLiteral("mail"),
        QStringLiteral("maps"),
        QStringLiteral("marketplace"),
        QStringLiteral("messages"),
        QStringLiteral("new"),
        QStringLiteral("news"),
        QStringLiteral("notifications"),
        QStringLiteral("oauth"),
        QStringLiteral("pricing"),
        QStringLiteral("privacy"),
        QStringLiteral("pulls"),
        QStringLiteral("search"),
        QStringLiteral("setting"),
        QStringLiteral("settings"),
        QStringLiteral("signin"),
        QStringLiteral("signout"),
        QStringLiteral("signup"),
        QStringLiteral("status"),
        QStringLiteral("support"),
        QStringLiteral("teams"),
        QStringLiteral("terms"),
        QStringLiteral("ticket"),
        QStringLiteral("tickets"),
        QStringLiteral("timer"),
        QStringLiteral("timers"),
        QStringLiteral("unattended"),
        QStringLiteral("watch"),
        QStringLiteral("www"),
    };
    return k;
}

QStringList pathSegments(const QString &path)
{
    QString p = path;
    if (p.startsWith(QLatin1Char('/'))) {
        p = p.mid(1);
    }
    while (p.endsWith(QLatin1Char('/'))) {
        p.chop(1);
    }
    if (p.isEmpty()) {
        return {};
    }
    QStringList segs = p.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    if (segs.size() > 6) {
        segs = segs.mid(0, 6);
    }
    return segs;
}

bool originWideScope(const QString &scope)
{
    if (scope.isEmpty()) {
        return true;
    }
    const QString path = parseUrl(scope).path;
    return path.isEmpty() || path == QLatin1String("/");
}

} // namespace

bool isAppChromeSegment(const QString &segment)
{
    return chromeSegments().contains(segment.trimmed().toLower());
}

QStringList destinationLadder(const QString &url)
{
    const ParsedUrl p = parseUrl(url);
    if (p.host.isEmpty()) {
        return {};
    }
    const QStringList segs = pathSegments(p.path);
    QStringList out;
    QString key = p.host;
    out << key;
    for (const auto &seg : segs) {
        key += QLatin1Char('/') + seg;
        out << key;
    }
    return out;
}

bool destinationKeyMatches(const QString &key, const QString &url)
{
    const ParsedUrl p = parseUrl(url);
    if (key.isEmpty() || p.host.isEmpty()) {
        return false;
    }
    if (key == p.host) {
        return true;
    }
    const QString prefix = p.host + QLatin1Char('/');
    if (!key.startsWith(prefix)) {
        return false;
    }
    const QString keyPath = QLatin1Char('/') + key.mid(prefix.size());
    QString urlPath = p.path.isEmpty() ? QStringLiteral("/") : p.path;
    if (urlPath.size() > 1 && urlPath.endsWith(QLatin1Char('/'))) {
        urlPath.chop(1);
    }
    return urlPath == keyPath || urlPath.startsWith(keyPath + QLatin1Char('/'));
}

QString destinationKeyMatchesBest(const QString &url, const QMap<QString, QString> &remembered)
{
    QString best;
    for (auto it = remembered.begin(); it != remembered.end(); ++it) {
        if (!destinationKeyMatches(it.key(), url)) {
            continue;
        }
        if (it.key().size() > best.size()) {
            best = it.key();
        }
    }
    return best;
}

QString lookupRemembered(const QString &url, const QMap<QString, QString> &remembered)
{
    const QString key = destinationKeyMatchesBest(url, remembered);
    if (key.isEmpty()) {
        return {};
    }
    return remembered.value(key);
}

int suggestedLadderIndex(const QString &url, const Target *target, const QMap<QString, QString> &remembered)
{
    const QStringList ladder = destinationLadder(url);
    if (ladder.isEmpty()) {
        return 0;
    }

    int idx = 0;
    const ParsedUrl p = parseUrl(url);
    const QStringList segs = pathSegments(p.path);
    const bool tenantPath = !segs.isEmpty() && !isAppChromeSegment(segs.first());

    if (target && target->kind == Kind::Pwa && !target->pwaScope.isEmpty()) {
        if (!originWideScope(target->pwaScope)) {
            const ParsedUrl scope = parseUrl(target->pwaScope);
            QString want = scope.host;
            for (const auto &s : pathSegments(scope.path)) {
                want += QLatin1Char('/') + s;
            }
            const int found = ladder.indexOf(want);
            if (found >= 0) {
                idx = found;
            }
        } else if (tenantPath) {
            idx = 1;
        }
    } else if (target && tenantPath) {
        const QString existing = remembered.value(p.host);
        if (!existing.isEmpty() && existing != target->id) {
            idx = 1;
        }
    }
    return qBound(0, idx, ladder.size() - 1);
}

bool pwaShouldAutoOpen(const Target &pwa, const QString &url)
{
    if (pwa.kind != Kind::Pwa || !urlInScope(url, pwa.pwaScope)) {
        return false;
    }
    if (!originWideScope(pwa.pwaScope)) {
        return true;
    }
    const QStringList segs = pathSegments(parseUrl(url).path);
    if (segs.isEmpty() || isAppChromeSegment(segs.first())) {
        return true;
    }
    return false;
}

} // namespace Tern
