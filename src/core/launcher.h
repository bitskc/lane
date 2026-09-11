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
// Walks every symlink hop starting at execPath (an existing, already
// resolved path -- see resolveExecutable()) to its final target, checking
// each hop's basename against isBlockedInterpreter(). A plain basename
// check on execPath alone would miss a blocked interpreter reached through
// an innocently named symlink. Fails closed: a hop that cannot be
// inspected (dangling link, deleted-out-from-under-us race, a chain
// longer than any real executable needs) is treated as blocked rather
// than silently passed through.
bool isBlockedInterpreterChain(const QString &execPath);
QString resolveExecutable(const QString &exec);
bool parseCustomCommand(const QString &command, Target *out, QString *error);

} // namespace Lane
