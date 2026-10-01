#pragma once

#include "types.h"

namespace Lane
{

Decision route(Click click, const QList<Target> &targets, const Config &config);
QList<Target> rankForPicker(const Click &click, const QList<Target> &targets, const Config &config);
const Target *findTarget(const QList<Target> &targets, const QString &id);
// Resolves the fallback target. When activityId names the current Plasma
// Activity (non-empty only while Activity routing is active), an entry in
// config.activityDefaults wins over config.defaultTargetId; with an empty
// activityId the lookup is exactly the historic global default.
const Target *defaultTarget(const QList<Target> &targets, const Config &config, const QString &activityId = QString());
QStringList danglingRememberedKeys(const QList<Target> &targets, const Config &config);

// Resolves the private/incognito sibling of a target for the picker's
// Alt+P shortcut. Already-incognito targets return themselves. Container
// targets (id suffixed ":container:N") resolve against their base
// profile's sibling. Matching is by exact profile id only
// (id + ":private" for Gecko, id + ":incognito" for Chromium) so a
// same-browser sibling with an unrelated purpose (Brave's ":tor" profile)
// is never mistaken for the counterpart. Any target with no such sibling
// in the list -- a PWA, a custom action, an unmatched browser -- fails
// closed: a default-constructed (empty-id) Target, never a guess.
Target privateCounterpart(const QList<Target> &targets, const Target &t);

// Grace-period decision for automatic remembered-host pruning: a target
// missing from one discovery pass is not necessarily uninstalled (a
// transient discovery false negative, e.g. a Flatpak export dir mid-update
// or a bare systemd unit with no XDG_DATA_DIRS, looks identical to the
// target actually being gone). Callers track how many consecutive
// reload()s each dangling remembered-host key has stayed dangling and only
// prune once that count reaches `threshold`. A manual, user-confirmed
// prune (e.g. the Settings "Clear dead" action) does not need this and
// should keep pruning everything danglingRememberedKeys() reports
// immediately instead of calling this.
bool shouldPruneRemembered(int missCount, int threshold);

} // namespace Lane
