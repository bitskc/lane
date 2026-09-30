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
    // High-confidence unique tracking keys. "ref" and the campaign-suite
    // prefixes below are noisy in the wild but are exactly what surveys
    // of newsletter/social/ad-platform links keep turning up.
    static const QSet<QString> kExactBanned = {
        QStringLiteral("fbclid"),      QStringLiteral("gclid"),      QStringLiteral("gbraid"),
        QStringLiteral("wbraid"),      QStringLiteral("msclkid"),    QStringLiteral("twclid"),
        QStringLiteral("mc_eid"),      QStringLiteral("mc_cid"),     QStringLiteral("mkt_tok"),
        QStringLiteral("_ga"),         QStringLiteral("_gl"),        QStringLiteral("dclid"),
        QStringLiteral("yclid"),       QStringLiteral("ttclid"),     QStringLiteral("li_fat_id"),
        QStringLiteral("_hsenc"),      QStringLiteral("_hsmi"),      QStringLiteral("oly_enc_id"),
        QStringLiteral("oly_anon_id"), QStringLiteral("vero_id"),    QStringLiteral("rb_clickid"),
        QStringLiteral("s_cid"),       QStringLiteral("wickedid"),   QStringLiteral("igshid"),
        QStringLiteral("ref"),
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

    const int queryIdx = url.indexOf(QLatin1Char('?'));
    if (queryIdx < 0) {
        return url;
    }

    const int fragmentIdx = url.indexOf(QLatin1Char('#'), queryIdx + 1);
    const QString prefix = url.left(queryIdx);
    const QString rawQuery = fragmentIdx < 0 ? url.mid(queryIdx + 1) : url.mid(queryIdx + 1, fragmentIdx - queryIdx - 1);
    const QString suffix = fragmentIdx < 0 ? QString() : url.mid(fragmentIdx);

    // hostOf() is a read-only QUrl::fromUserInput() parse used purely to
    // decide the "si" host allowlist below; it never feeds back into the
    // reconstructed string, so it cannot corrupt anything.
    const bool siTracked = isSiTrackedHost(hostOf(url));

    QStringList kept;
    const QStringList segments = rawQuery.split(QRegularExpression(QStringLiteral("[&;]")));
    for (const QString &segment : segments) {
        if (segment.isEmpty()) {
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
            if (removedKeys) {
                *removedKeys << rawKey;
            }
            continue;
        }
        kept << segment;
    }

    if (kept.isEmpty()) {
        return prefix + suffix;
    }
    return prefix + QLatin1Char('?') + kept.join(QLatin1Char('&')) + suffix;
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

    // "working" (post-unwrap/unshorten/substitution, pre-tracker-strip) is
    // what openUrl is built from below. Tracking-parameter stripping only
    // ever lands in matchUrl: openUrl must stay launchable exactly as the
    // O365/substitution stage left it, tracking junk included, so a
    // wrapper that embeds its target in a query value never gets its
    // encoding disturbed by the cleaner.
    if (config.stripTrackingParams) {
        click.matchUrl = stripTrackingParams(working, &click.removedTrackingParams);
    } else {
        click.matchUrl = working;
    }

    const bool wrapped = isO365Wrapper(click.originalUrl);
    if (wrapped && !config.openUnwrapped) {
        click.openUrl = click.originalUrl;
    } else {
        click.openUrl = working;
    }

    click.host = hostOf(click.matchUrl);
    click.path = pathOf(click.matchUrl);
    return click;
}

} // namespace Lane
