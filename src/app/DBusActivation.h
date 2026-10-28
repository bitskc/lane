#pragma once

#include <QString>
#include <QStringList>

#include <functional>

namespace Lane
{

// Registers the live D-Bus activation contract this replaces KDBusService
// for: org.freedesktop.Application (Activate/Open/ActivateAction) and
// org.kde.KDBusService.CommandLine, exported on both /app/lane/Lane and
// /MainApplication (parity with KDBusService's own export, verified via
// qdbus6 introspection of the previous build).
//
// `handleArgs` is invoked for CommandLine's argv, for Activate (called
// with an empty list - the same "no args" shape Controller::handleArgs
// already handles for a plain daemon start), and for ActivateAction (called
// with the argv mapped from the desktop action name by argsForAction).
// `openUrl` is invoked once per URI for Open.
//
// Each handler sets XDG_ACTIVATION_TOKEN from platform_data
// ["activation-token"] (falling back to ["desktop-startup-id"]) for the
// duration of the callback, then unsets it, mirroring KDBusService's
// documented env-token behavior that Controller::openUrl depends on.
//
// Returns false if the session bus is unavailable or the well-known name
// "app.lane.Lane" cannot be claimed - fatal in daemon mode, since bus-name
// acquisition is the systemd Type=dbus readiness signal (same semantic as
// KDBusService::Unique).
bool registerActivation(std::function<void(const QStringList &args)> handleArgs,
                        std::function<void(const QString &url)> openUrl);

// Maps a freedesktop / KDE desktop action name (from .desktop Actions) to
// the corresponding command-line argument list.
QStringList argsForAction(const QString &actionName);

} // namespace Lane
