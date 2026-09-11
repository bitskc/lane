#pragma once

#include "types.h"

class QString;

namespace Lane
{

Config loadConfig(const QString &path);
bool saveConfig(const QString &path, const Config &config);
QString defaultConfigPath();

// One-time migration from the pre-rename config location
// (~/.config/tern/config.json, superseded) to the current one
// (~/.config/lane/config.json). Copies, never moves: the legacy file is
// left untouched so it survives being run more than once and can never
// lose data. Does nothing if the new path already has a file, or if the
// legacy path has none.
void migrateLegacyConfig();

// Same logic as migrateLegacyConfig(), against explicit paths rather than
// the real ~/.config locations. Exists so tests can exercise the actual
// migration behavior (idempotency, never-overwrite-an-existing-file,
// source left intact on every outcome including failure) without ever
// touching a real user's config directory.
void migrateLegacyConfig(const QString &oldPath, const QString &newPath);

QString pickerPolicyToString(PickerPolicy p);
PickerPolicy pickerPolicyFromString(const QString &s);
QString scopeToString(MatchScope s);
MatchScope scopeFromString(const QString &s);
QString locationToString(MatchLocation l);
MatchLocation locationFromString(const QString &s);

} // namespace Lane
