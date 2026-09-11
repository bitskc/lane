#pragma once

#include "types.h"

namespace Tern
{

QStringList expandArgs(const Target &target, const QString &url);
bool launchTarget(const Target &target, const QString &url);
bool isBlockedInterpreter(const QString &exec);
QString resolveExecutable(const QString &exec);
bool parseCustomCommand(const QString &command, Target *out, QString *error);

} // namespace Tern
