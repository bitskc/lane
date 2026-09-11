#include "pipeline.h"

#include "urlutil.h"

#include <QRegularExpression>

namespace Tern
{

Click runPipeline(const QString &rawUrl, const Config &config, const UnshortenFn &unshorten)
{
    Click click;
    click.originalUrl = normalizeInput(rawUrl);
    QString working = click.originalUrl;

    if (config.unwrapO365) {
        working = unwrapO365(working);
    }

    if (config.unshorten && unshorten && isShortener(working) && isSafeOpenUrl(working)) {
        const QString expanded = unshorten(working);
        if (isSafeOpenUrl(expanded) && !isPrivateOrLocalHost(hostOf(expanded))) {
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

} // namespace Tern
