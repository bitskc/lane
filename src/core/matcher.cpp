#include "matcher.h"

#include "urlutil.h"

#include <QRegularExpression>

namespace Tern
{

static QString haystack(const Rule &rule, const Click &click)
{
    switch (rule.location) {
    case MatchLocation::WindowTitle:
        return click.windowTitle;
    case MatchLocation::ProcessName:
        return click.processName;
    case MatchLocation::Url:
        break;
    }

    if (rule.scope == MatchScope::Domain) {
        return hostOf(click.matchUrl);
    }
    if (rule.scope == MatchScope::Path) {
        return pathOf(click.matchUrl);
    }
    return click.matchUrl;
}

bool ruleMatches(const Rule &rule, const Click &click)
{
    if (!rule.enabled || rule.pattern.trimmed().isEmpty()) {
        return false;
    }
    const QString input = haystack(rule, click).trimmed();
    if (input.isEmpty()) {
        return false;
    }
    if (rule.regex) {
        const QRegularExpression re(rule.pattern, QRegularExpression::CaseInsensitiveOption);
        if (!re.isValid()) {
            return false;
        }
        // BT regexes match the entire scoped input.
        return re.match(input).hasMatch() && re.match(input).capturedLength() == input.size();
    }
    return input.contains(rule.pattern, Qt::CaseInsensitive);
}

} // namespace Tern
