#include "DefaultBrowserWatcher.h"

#include <QFileInfo>
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
    // A candidate file that does not exist when we are enabled can be
    // created later (first browser install, desktop writing the legacy
    // path for the first time). addPath() on a missing file is a no-op
    // and fileChanged can never fire for it, so the parent directory is
    // watched instead and rescanPaths() re-arms the file once it shows
    // up — and reports the change, since a new mimeapps.list is exactly
    // the event this watcher exists for.
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, [this] {
        // Only a newly-appearing mimeapps.list is interesting — churn in
        // the parent dir (unrelated file writes) must not fire the
        // takeover check.
        if (m_enabled && rescanPaths()) {
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
        const QStringList watchedDirs = m_watcher.directories();
        if (!watchedDirs.isEmpty()) {
            m_watcher.removePaths(watchedDirs);
        }
        m_debounce.stop();
    }
}

bool DefaultBrowserWatcher::rescanPaths()
{
    // addPath() is a no-op for paths already watched, so calling this on
    // every change both picks up files created after start and re-arms
    // ones that were atomically replaced.
    bool appeared = false;
    for (const QString &p : m_paths) {
        if (QFileInfo::exists(p)) {
            if (!m_watchedFiles.contains(p)) {
                appeared = true;
            }
            m_watcher.addPath(p);
            m_watchedFiles.insert(p);
        } else {
            // The file is not there — watch its parent directory so the
            // first write lands as a directoryChanged and re-arms the
            // file watch here.
            m_watchedFiles.remove(p);
            const QString dir = QFileInfo(p).absolutePath();
            if (!dir.isEmpty() && QFileInfo::exists(dir)) {
                m_watcher.addPath(dir);
            }
        }
    }
    return appeared;
}
