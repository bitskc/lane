#pragma once

#include "types.h"

class QString;

namespace Tern
{

Config loadConfig(const QString &path);
bool saveConfig(const QString &path, const Config &config);
QString defaultConfigPath();

QString pickerPolicyToString(PickerPolicy p);
PickerPolicy pickerPolicyFromString(const QString &s);
QString scopeToString(MatchScope s);
MatchScope scopeFromString(const QString &s);
QString locationToString(MatchLocation l);
MatchLocation locationFromString(const QString &s);

} // namespace Tern
