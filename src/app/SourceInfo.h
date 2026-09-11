#pragma once

#include <QString>

namespace Tern
{

struct SourceInfo {
    QString processName;
    QString windowTitle;
};

SourceInfo activeSource();

} // namespace Tern
