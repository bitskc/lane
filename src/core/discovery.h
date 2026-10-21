#pragma once

#include "types.h"

class QProcess;

namespace Lane
{

struct DiscoveryPaths {
    QStringList applicationDirs;
    QString configHome;
    QString dataHome;
    QString home;
    // macOS only: directories scanned for .app bundles (one level deep,
    // no recursion) and the base for macOS profile-root candidates
    // (~/Library/Application Support). Both stay empty on Linux.
    // `applicationSupport` non-empty is what selects the macOS side of
    // the profile-root table, so the table stays exercisable in Linux
    // tests without a real macOS filesystem.
    QStringList bundleDirs;
    QString applicationSupport;
};

DiscoveryPaths defaultDiscoveryPaths();
QList<Target> discoverTargets(const DiscoveryPaths &paths);
// Path of the discovered .app for `bundleId`, empty if that id was not
// kept. ThemeIconProvider uses this to resolve `image://laneicon/bundle:<id>`
// through QFileIconProvider without putting the path in the icon key.
QString macBundlePathForId(const QString &bundleId);
QList<Target> applyConfigToTargets(QList<Target> targets, const Config &config);
QStringList moveIdAmongSiblings(const QList<Target> &targets, const QString &id, int newIndex);

// Reads a running process's stdout up to maxBytes, killing it if the
// bound or timeout is hit. Used by binary Info.plist conversion so a
// hostile expansion cannot buffer unboundedly in QProcess first.
QByteArray readStdoutBounded(QProcess &proc, qsizetype maxBytes, int timeoutMs);

} // namespace Lane
