#pragma once

#include "types.h"

namespace Tern
{

Decision route(Click click, const QList<Target> &targets, const Config &config);
QList<Target> rankForPicker(const Click &click, const QList<Target> &targets, const Config &config);
const Target *findTarget(const QList<Target> &targets, const QString &id);
const Target *defaultTarget(const QList<Target> &targets, const Config &config);

} // namespace Tern
