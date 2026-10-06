#include "../src/app/DefaultBrowserWatcher.h"

#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

class WatchdogTest : public QObject
{
    Q_OBJECT

private:
    static QString writeMimeapps(const QTemporaryDir &dir, const QString &contents)
    {
        const QString path = dir.filePath(QStringLiteral("mimeapps.list"));
        QFile f(path);
        f.open(QIODevice::WriteOnly | QIODevice::Truncate);
        f.write(contents.toUtf8());
        f.close();
        return path;
    }

private Q_SLOTS:
    void disabledWatcherNeverFires()
    {
        QTemporaryDir dir;
        const QString path = writeMimeapps(dir, QStringLiteral("x-scheme-handler/http=app.lane.Lane.desktop\n"));

        DefaultBrowserWatcher watcher({path});
        QSignalSpy spy(&watcher, &DefaultBrowserWatcher::mimeappsChanged);
        // Disabled by default: a rewrite must produce nothing.
        writeMimeapps(dir, QStringLiteral("x-scheme-handler/http=firefox.desktop\n"));
        QVERIFY(!spy.wait(2500));
    }

    void rewriteFiresOnceDebounced()
    {
        QTemporaryDir dir;
        const QString path = writeMimeapps(dir, QStringLiteral("x-scheme-handler/http=app.lane.Lane.desktop\n"));

        DefaultBrowserWatcher watcher({path});
        watcher.setEnabled(true);
        QSignalSpy spy(&watcher, &DefaultBrowserWatcher::mimeappsChanged);

        // Two quick writes still collapse into one debounced signal.
        writeMimeapps(dir, QStringLiteral("x-scheme-handler/http=firefox.desktop\n"));
        writeMimeapps(dir, QStringLiteral("x-scheme-handler/http=firefox.desktop\nx-scheme-handler/https=firefox.desktop\n"));
        QVERIFY(spy.wait(3000));
        QCOMPARE(spy.count(), 1);
    }

    void disablingMidDebounceStopsTheSignal()
    {
        QTemporaryDir dir;
        const QString path = writeMimeapps(dir, QStringLiteral("x-scheme-handler/http=app.lane.Lane.desktop\n"));

        DefaultBrowserWatcher watcher({path});
        watcher.setEnabled(true);
        writeMimeapps(dir, QStringLiteral("x-scheme-handler/http=firefox.desktop\n"));
        // Turn it off inside the debounce window; the pending signal must die.
        watcher.setEnabled(false);
        QSignalSpy spy(&watcher, &DefaultBrowserWatcher::mimeappsChanged);
        QVERIFY(!spy.wait(2500));
    }

    void fileCreatedAfterEnableFires()
    {
        // The candidate path does not exist when the watcher is enabled;
        // a browser install writing it later must still be caught via the
        // parent-directory watch.
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("mimeapps.list"));
        QVERIFY(!QFile::exists(path));

        DefaultBrowserWatcher watcher({path});
        watcher.setEnabled(true);
        QSignalSpy spy(&watcher, &DefaultBrowserWatcher::mimeappsChanged);

        writeMimeapps(dir, QStringLiteral("x-scheme-handler/http=firefox.desktop\n"));
        QVERIFY(spy.wait(3500));
        QCOMPARE(spy.count(), 1);
    }
};

QTEST_MAIN(WatchdogTest)
#include "test_watchdog.moc"
