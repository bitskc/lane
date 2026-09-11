#pragma once

#include "types.h"

namespace Tern
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

} // namespace Tern
