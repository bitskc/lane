#pragma once

#include <QStringList>

namespace Lane
{

// A forwarded argv always includes argv[0]. On macOS, only a secondary
// Finder/Spotlight launch with nothing after argv[0] opens Settings. Commands
// such as --clipboard own the window they open and must retain its focus.
inline bool isBareForwardedInvocation(const QStringList &args)
{
    return args.size() <= 1;
}

} // namespace Lane
