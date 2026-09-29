#pragma once

#include "types.h"

namespace Lane
{

Decision route(Click click, const QList<Target> &targets, const Config &config);
QList<Target> rankForPicker(const Click &click, const QList<Target> &targets, const Config &config);
const Target *findTarget(const QList<Target> &targets, const QString &id);
const Target *defaultTarget(const QList<Target> &targets, const Config &config);
QStringList danglingRememberedKeys(const QList<Target> &targets, const Config &config);

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
