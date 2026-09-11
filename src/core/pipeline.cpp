#include "pipeline.h"

#include "urlutil.h"

#include <QRegularExpression>
#include <QSet>

namespace Lane
{

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

    click.matchUrl = working;
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
