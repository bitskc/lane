#pragma once

#include "types.h"

#include <functional>

namespace Tern
{

using UnshortenFn = std::function<QString(const QString &)>;

Click runPipeline(const QString &rawUrl, const Config &config, const UnshortenFn &unshorten = {});

} // namespace Tern
