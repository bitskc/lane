#pragma once

#include <QString>

namespace Lane
{

struct SourceInfo {
    QString processName;
    QString windowTitle;
};

SourceInfo activeSource();

} // namespace Lane
