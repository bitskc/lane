#pragma once

#include <QString>

namespace Lane
{

// Three-valued comparison result for two version strings, plus an explicit
// Unknown for input that cannot be parsed with confidence. Never guess.
enum class VersionOrder {
    Older,
    Same,
    Newer,
    Unknown,
};

// Compares two version strings of the form "[v]MAJOR[.MINOR[.PATCH...]][-prerelease]".
// - A leading "v"/"V" is optional and ignored.
// - Segment counts may differ ("0.2" vs "0.1.0"); missing trailing segments
//   are treated as zero.
// - Numeric segments are compared as numbers, not text, so "0.10.0" is newer
//   than "0.9.0".
// - A prerelease suffix ("0.2.0-rc1") sorts before the same version without
//   one ("0.2.0"); two prerelease suffixes are compared identifier by
//   identifier the way semver does.
// - Anything that fails to parse as a version on either side returns
//   Unknown rather than a best-effort guess.
VersionOrder compareVersions(const QString &a, const QString &b);

} // namespace Lane
