#pragma once

#include "types.h"

#include <optional>

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
// The macOS superset of isBlockedInterpreterChain(): the same basename and
// symlink-chain walk plus the macOS-only exec-name additions (open,
// osascript, swift, script, tclsh, wish, shortcuts, lldb, automator), and it
// also refuses a chain whose path resolves to a directory or ends in .app.
// These are the rules the macOS build runs; exported so tests can pin the
// macOS set on any platform. parseCustomCommand() and launchTarget() call
// this on macOS and isBlockedInterpreterChain() on Linux; a Linux `open`
// command keeps its old behavior.
bool isBlockedInterpreterChainMac(const QString &execPath);

// The fixed /usr/bin/open routes a macOS build takes instead of exec'ing
// target.exec. Returns no value for a target the route does not own, and
// an EMPTY list for a target it owns but must refuse (a launch of
// /usr/bin/open with no argument would open Finder). Program is always
// /usr/bin/open: launchTarget() never takes its path or argv text from
// target.exec, target.args or config. Pure QString work, portable; the
// /usr/bin/open call site itself is behind Q_OS_MACOS.
//  - action:email: [mailto:?body=<percent-encoded url>]; the whole
//    sanitized URL is encoded so an attacker query cannot inject to, cc
//    or subject parameters.
//  - Safari bundle (bundleId com.apple.Safari): [-b, bundleId, url];
//    Safari takes no profile or private arguments.
std::optional<QStringList> macFixedOpenArgs(const Target &target, const QString &sanitizedUrl);

QString resolveExecutable(const QString &exec);
bool parseCustomCommand(const QString &command, Target *out, QString *error);

} // namespace Lane
