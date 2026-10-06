#pragma once

#include <QFileSystemWatcher>
#include <QObject>
#include <QSet>
#include <QStringList>
#include <QTimer>

// Watches the mimeapps.list files that record x-scheme-handler defaults
// and emits mimeappsChanged() (debounced) whenever any of them is
// rewritten. It deliberately reports only that the file changed; the
// Controller decides whether Lane actually lost the handler, so a user
// who never opted Lane in is never bothered.
class DefaultBrowserWatcher : public QObject
{
    Q_OBJECT

public:
    // The XDG paths where desktops record default applications.
    static QStringList mimeappsPaths();

    explicit DefaultBrowserWatcher(QObject *parent = nullptr);
    // Test hook: watch a caller-provided set of files instead of the
    // real XDG locations.
    explicit DefaultBrowserWatcher(const QStringList &paths, QObject *parent = nullptr);

    void setEnabled(bool on);
    bool isEnabled() const { return m_enabled; }

Q_SIGNALS:
    void mimeappsChanged();

private:
    // Re-arms watches; returns true if a watched file just appeared
    // (directoryChanged uses this to distinguish a new mimeapps.list
    // from unrelated churn in the same folder).
    bool rescanPaths();

    QStringList m_paths;
    QSet<QString> m_watchedFiles;
    QFileSystemWatcher m_watcher;
    // Editor swaps mean the path can vanish and reappear, and several
    // writers can touch the file in quick succession; debounce so one
    // takeover produces one signal.
    QTimer m_debounce;
    bool m_enabled = false;
};
