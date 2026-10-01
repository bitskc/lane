#pragma once

#include "types.h"

namespace Lane
{

bool ruleMatches(const Rule &rule, const Click &click);

// Plasma Activity scope gate for a rule. A rule with no activity set is
// in scope everywhere. When no current Activity is known (routing
// disabled, non-Plasma desktop, or the activities service still
// starting up) every rule passes, so behavior matches Lane without
// Activity support. Otherwise a scoped rule is in scope when its
// activity value equals the current Activity's ID or its name.
bool ruleActivityMatches(const Rule &rule, const QString &currentActivityId, const QString &currentActivityName);

} // namespace Lane
