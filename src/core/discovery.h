#pragma once

#include "types.h"

namespace Lane
{

struct DiscoveryPaths {
    QStringList applicationDirs;
    QString configHome;
    QString dataHome;
    QString home;
};

DiscoveryPaths defaultDiscoveryPaths();
QList<Target> discoverTargets(const DiscoveryPaths &paths);
QList<Target> applyConfigToTargets(QList<Target> targets, const Config &config);
QStringList moveIdAmongSiblings(const QList<Target> &targets, const QString &id, int newIndexInKind);

} // namespace Lane
