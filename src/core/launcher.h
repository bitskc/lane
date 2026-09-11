#pragma once

#include "types.h"

namespace Lane
{

QStringList expandArgs(const Target &target, const QString &url);
// activationToken, when non-empty, is set as XDG_ACTIVATION_TOKEN in Lane's
// own environment for the narrow duration of the spawn (see launcher.cpp)
// so a Wayland compositor will let the launched target take focus.
bool launchTarget(const Target &target, const QString &url, const QString &activationToken = QString());
bool isBlockedInterpreter(const QString &exec);
QString resolveExecutable(const QString &exec);
bool parseCustomCommand(const QString &command, Target *out, QString *error);

} // namespace Lane
