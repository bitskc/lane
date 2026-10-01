#include "DefaultBrowserWatcher.h"

#include <QStandardPaths>

QStringList DefaultBrowserWatcher::mimeappsPaths()
{
    // Both locations hold x-scheme-handler defaults depending on the
    // distro and tool: the shared-mime spec path under XDG_CONFIG_HOME
    // and the legacy fallback inside the applications data dir.
    const QString config = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    const QString data = QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation);
    return {
        config + QStringLiteral("/mimeapps.list"),
        data + QStringLiteral("/mimeapps.list"),
    };
}

DefaultBrowserWatcher::DefaultBrowserWatcher(QObject *parent)
    : DefaultBrowserWatcher(mimeappsPaths(), parent)
{
}

DefaultBrowserWatcher::DefaultBrowserWatcher(const QStringList &paths, QObject *parent)
    : QObject(parent)
    , m_paths(paths)
{
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(2000);
    connect(&m_debounce, &QTimer::timeout, this, &DefaultBrowserWatcher::mimeappsChanged);

    // QFileSystemWatcher drops a path it can no longer see (the writer
    // replaced the file), so rescan on every hit and on deletions.
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, [this] {
        rescanPaths();
        if (m_enabled) {
            m_debounce.start();
        }
    });
}

void DefaultBrowserWatcher::setEnabled(bool on)
{
    if (m_enabled == on) {
        return;
    }
    m_enabled = on;
    if (on) {
        rescanPaths();
    } else {
        const QStringList watched = m_watcher.files();
        if (!watched.isEmpty()) {
            m_watcher.removePaths(watched);
        }
        m_debounce.stop();
    }
}

void DefaultBrowserWatcher::rescanPaths()
{
    // addPath() is a no-op for paths already watched, so calling this on
    // every change both picks up files created after start and re-arms
    // ones that were atomically replaced.
    for (const QString &p : m_paths) {
        m_watcher.addPath(p);
    }
}
