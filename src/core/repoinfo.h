#pragma once

#include <QString>

namespace Lane
{

// Owner/repo slug for the public GitHub project. Single source of truth: a
// repository rename is a one-line change here instead of a hunt through
// string literals in the update checker and the settings UI.
inline constexpr const char *kGitHubRepoSlug = "bitskc/lane";

inline QString githubProjectUrl()
{
    return QStringLiteral("https://github.com/%1").arg(QLatin1String(kGitHubRepoSlug));
}

inline QString githubLatestReleaseApiUrl()
{
    return QStringLiteral("https://api.github.com/repos/%1/releases/latest").arg(QLatin1String(kGitHubRepoSlug));
}

} // namespace Lane
