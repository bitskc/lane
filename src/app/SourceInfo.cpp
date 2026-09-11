#include "SourceInfo.h"

namespace Tern
{

SourceInfo activeSource()
{
    // Plasma 6 Wayland does not expose a portable active-window PID.
    // Process/title rules still work when the caller is known by other means.
    return {};
}

} // namespace Tern
