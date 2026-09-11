#include "version.h"

#include <QStringList>

namespace Lane
{

namespace
{

struct ParsedVersion {
    QList<qint64> segments;
    QStringList prerelease; // empty means "no prerelease" (a release build)
    bool valid = false;
};

bool isAllDigits(const QString &s)
{
    if (s.isEmpty()) {
        return false;
    }
    for (const QChar &c : s) {
        if (!c.isDigit()) {
            return false;
        }
    }
    return true;
}

ParsedVersion parse(const QString &input)
{
    ParsedVersion result;
    QString s = input.trimmed();
    if (s.isEmpty()) {
        return result;
    }
    if (s.startsWith(QLatin1Char('v')) || s.startsWith(QLatin1Char('V'))) {
        s = s.mid(1);
    }
    if (s.isEmpty()) {
        return result;
    }

    // Build metadata ("+...") carries no ordering information; drop it.
    const int plusIdx = s.indexOf(QLatin1Char('+'));
    if (plusIdx >= 0) {
        s = s.left(plusIdx);
    }

    QString release = s;
    const int dashIdx = s.indexOf(QLatin1Char('-'));
    if (dashIdx >= 0) {
        release = s.left(dashIdx);
        const QString pre = s.mid(dashIdx + 1);
        if (pre.isEmpty()) {
            return result;
        }
        result.prerelease = pre.split(QLatin1Char('.'));
        for (const QString &id : std::as_const(result.prerelease)) {
            if (id.isEmpty()) {
                return result;
            }
        }
    }

    if (release.isEmpty()) {
        return result;
    }
    const QStringList parts = release.split(QLatin1Char('.'));
    for (const QString &part : parts) {
        if (!isAllDigits(part)) {
            return result;
        }
        bool ok = false;
        const qint64 value = part.toLongLong(&ok);
        if (!ok) {
            return result;
        }
        result.segments.append(value);
    }
    if (result.segments.isEmpty()) {
        return result;
    }
    result.valid = true;
    return result;
}

int compareSegments(const QList<qint64> &a, const QList<qint64> &b)
{
    const int n = a.size() > b.size() ? a.size() : b.size();
    for (int i = 0; i < n; ++i) {
        const qint64 av = i < a.size() ? a[i] : 0;
        const qint64 bv = i < b.size() ? b[i] : 0;
        if (av != bv) {
            return av < bv ? -1 : 1;
        }
    }
    return 0;
}

int comparePrereleaseIdentifier(const QString &a, const QString &b)
{
    const bool aNum = isAllDigits(a);
    const bool bNum = isAllDigits(b);
    if (aNum && bNum) {
        const qint64 av = a.toLongLong();
        const qint64 bv = b.toLongLong();
        if (av != bv) {
            return av < bv ? -1 : 1;
        }
        return 0;
    }
    if (aNum != bNum) {
        // Semver rule: numeric identifiers always sort lower than alphanumeric ones.
        return aNum ? -1 : 1;
    }
    if (a == b) {
        return 0;
    }
    return a.compare(b) < 0 ? -1 : 1;
}

int comparePrerelease(const QStringList &a, const QStringList &b)
{
    // No prerelease sorts after any prerelease: "0.2.0" is newer than "0.2.0-rc1".
    if (a.isEmpty() && b.isEmpty()) {
        return 0;
    }
    if (a.isEmpty()) {
        return 1;
    }
    if (b.isEmpty()) {
        return -1;
    }
    const int n = a.size() < b.size() ? a.size() : b.size();
    for (int i = 0; i < n; ++i) {
        const int c = comparePrereleaseIdentifier(a[i], b[i]);
        if (c != 0) {
            return c;
        }
    }
    if (a.size() != b.size()) {
        return a.size() < b.size() ? -1 : 1;
    }
    return 0;
}

} // namespace

VersionOrder compareVersions(const QString &a, const QString &b)
{
    const ParsedVersion pa = parse(a);
    const ParsedVersion pb = parse(b);
    if (!pa.valid || !pb.valid) {
        return VersionOrder::Unknown;
    }
    int c = compareSegments(pa.segments, pb.segments);
    if (c == 0) {
        c = comparePrerelease(pa.prerelease, pb.prerelease);
    }
    if (c < 0) {
        return VersionOrder::Older;
    }
    if (c > 0) {
        return VersionOrder::Newer;
    }
    return VersionOrder::Same;
}

} // namespace Lane
