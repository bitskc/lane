#pragma once

#include "types.h"

namespace Tern
{

QStringList expandArgs(const Target &target, const QString &url);
bool launchTarget(const Target &target, const QString &url);

} // namespace Tern
