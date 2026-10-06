#include "pipeline.h"

#include "urlutil.h"

#include <QByteArray>
#include <QRegularExpression>
#include <QSet>

namespace Lane
{

namespace
{

bool isBannedTrackingKey(const QString &decodedKey)
{
    // High-confidence unique tracking keys: the campaign-suite prefixes
    // below plus exactly what surveys of newsletter/social/ad-platform
    // links keep turning up. "ref" is intentionally absent; it is a real
    // functional parameter (GitHub/GitLab branch refs), not a tracker.
    static const QSet<QString> kExactBanned = {
        QStringLiteral("fbclid"),      QStringLiteral("gclid"),      QStringLiteral("gbraid"),
        QStringLiteral("wbraid"),      QStringLiteral("msclkid"),    QStringLiteral("twclid"),
        QStringLiteral("mc_eid"),      QStringLiteral("mc_cid"),     QStringLiteral("mkt_tok"),
        QStringLiteral("_ga"),         QStringLiteral("_gl"),        QStringLiteral("dclid"),
        QStringLiteral("yclid"),       QStringLiteral("ttclid"),     QStringLiteral("li_fat_id"),
        QStringLiteral("_hsenc"),      QStringLiteral("_hsmi"),      QStringLiteral("oly_enc_id"),
        QStringLiteral("oly_anon_id"), QStringLiteral("vero_id"),    QStringLiteral("rb_clickid"),
        QStringLiteral("s_cid"),       QStringLiteral("wickedid"),   QStringLiteral("igshid"),
    };
    const QString lower = decodedKey.toLower();
    if (lower.startsWith(QLatin1String("utm_")) || lower.startsWith(QLatin1String("hsa_"))) {
        return true;
    }
    return kExactBanned.contains(lower);
}

bool isSiTrackedHost(const QString &host)
{
    static const QSet<QString> kSiHosts = {
        QStringLiteral("youtube.com"),
        QStringLiteral("youtu.be"),
        QStringLiteral("music.youtube.com"),
        QStringLiteral("open.spotify.com"),
    };
    return kSiHosts.contains(host);
}

} // namespace

QString stripTrackingParams(const QString &url, QStringList *removedKeys)
{
    if (removedKeys) {
        removedKeys->clear();
    }

    // Scheme guard, read straight off the raw text (never through QUrl):
    // mailto:, file:, tel:, etc. pass through completely untouched.
    const int colonIdx = url.indexOf(QLatin1Char(':'));
    if (colonIdx <= 0) {
        return url;
    }
    const QString scheme = url.left(colonIdx).toLower();
    if (scheme != QLatin1String("http") && scheme != QLatin1String("https")) {
        return url;
    }

    // Locate '#' first so a '?' that only appears inside the fragment
    // (e.g. https://app/#/route?utm_source=x) is never mistaken for the
    // query start. The query must live between the ':' scheme and '#'.
    const int fragmentIdx = url.indexOf(QLatin1Char('#'), colonIdx);
    const int queryEnd = fragmentIdx < 0 ? url.size() : fragmentIdx;
    const int queryIdx = url.indexOf(QLatin1Char('?'), colonIdx);
    if (queryIdx < 0 || queryIdx >= queryEnd) {
        return url;
    }

    const QString prefix = url.left(queryIdx);
    const QString rawQuery = url.mid(queryIdx + 1, queryEnd - queryIdx - 1);
    const QString suffix = url.mid(queryEnd); // "" when there is no fragment

    // hostOf() is a read-only QUrl::fromUserInput() parse used purely to
    // decide the "si" host allowlist below; it never feeds back into the
    // reconstructed string, so it cannot corrupt anything.
    const bool siTracked = isSiTrackedHost(hostOf(url));

    // Split manually so each segment keeps the separator that preceded it.
    // Rejoining with a fixed '&' would silently rewrite "a=1;b=2" to
    // "a=1&b=2" and collapse "&&" even when nothing was dropped. We also
    // return the input untouched when no tracker was removed, so a bare
    // trailing '?' or a ';'-separated query survives byte-for-byte.
    bool dropped = false;
    QString rebuilt;
    bool firstKept = true;
    QChar prevSep = QLatin1Char('\0');
    int start = 0;
    for (int i = 0; i <= rawQuery.size(); ++i) {
        const QChar c = i < rawQuery.size() ? rawQuery[i] : QLatin1Char('\0');
        const bool atSep = i == rawQuery.size() || c == QLatin1Char('&') || c == QLatin1Char(';');
        if (!atSep) {
            continue;
        }
        const QString segment = rawQuery.mid(start, i - start);
        const QChar segSep = (i < rawQuery.size()) ? c : QLatin1Char('\0');
        start = i + 1;
        if (segment.isEmpty()) {
            prevSep = segSep;
            continue;
        }
        const int eqIdx = segment.indexOf(QLatin1Char('='));
        const QString rawKey = eqIdx < 0 ? segment : segment.left(eqIdx);
        const QString decodedKey = QString::fromUtf8(QByteArray::fromPercentEncoding(rawKey.toUtf8()));

        bool drop = isBannedTrackingKey(decodedKey);
        if (!drop && siTracked && decodedKey.compare(QStringLiteral("si"), Qt::CaseInsensitive) == 0) {
            drop = true;
        }

        if (drop) {
            dropped = true;
            if (removedKeys) {
                *removedKeys << rawKey;
            }
        } else {
            if (!firstKept) {
                rebuilt += prevSep.isNull() ? QLatin1Char('&') : prevSep;
            }
            rebuilt += segment;
            firstKept = false;
        }
        prevSep = segSep;
    }

    if (!dropped) {
        return url; // no tracker removed: leave every byte alone
    }
    if (rebuilt.isEmpty()) {
        return prefix + suffix; // dropped everything: remove the '?' too
    }
    return prefix + QLatin1Char('?') + rebuilt + suffix;
}

Click runPipeline(const QString &rawUrl, const Config &config, const UnshortenFn &unshorten)
{
    Click click;
    click.originalUrl = normalizeInput(rawUrl);
    QString working = click.originalUrl;

    if (config.unwrapO365) {
        working = unwrapO365(working);
    }

    if (config.unshorten && unshorten) {
        static constexpr int kMaxUnshortenHops = 4;
        QSet<QString> visited;
        for (int hop = 0; hop < kMaxUnshortenHops; ++hop) {
            if (!isShortener(working) || !isSafeOpenUrl(working)) {
                break;
            }
            if (visited.contains(working)) {
                // Redirect loop: A -> B -> A. Stop where we are rather than spin.
                break;
            }
            visited.insert(working);
            const QString expanded = unshorten(working);
            if (expanded == working || !isSafeOpenUrl(expanded) || isPrivateOrLocalHost(hostOf(expanded))) {
                break;
            }
            working = expanded;
        }
    }

    for (const auto &sub : config.substitutions) {
        if (sub.find.isEmpty() || sub.find.size() > 128) {
            continue;
        }
        if (sub.regex) {
            const QRegularExpression re(sub.find);
            if (re.isValid()) {
                working.replace(re, sub.replace);
            }
        } else {
            working.replace(sub.find, sub.replace, Qt::CaseInsensitive);
        }
    }

    // The cleaner is byte-exact (only dropped segments change), so the
    // browser gets the clean URL too. An O365 wrapper still opens as the
    // untouched original below unless openUnwrapped is set.
    if (config.stripTrackingParams) {
        click.matchUrl = stripTrackingParams(working, &click.removedTrackingParams);
    } else {
        click.matchUrl = working;
    }

    const bool wrapped = isO365Wrapper(click.originalUrl);
    if (wrapped && !config.openUnwrapped) {
        click.openUrl = click.originalUrl;
    } else {
        click.openUrl = click.matchUrl;
    }

    click.host = hostOf(click.matchUrl);
    click.path = pathOf(click.matchUrl);
    return click;
}

} // namespace Lane
