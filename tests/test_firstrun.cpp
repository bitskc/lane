// First-run flag (design D2). Settings is the onboarding, so it opens at
// most once: an explicit Settings open consumes the flag, and the startup
// hook must not open it again. A config file that already exists is not a
// first run. Fails if openSettings() leaves the flag set.

#include "app/Controller.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QWindow>

#include <QtQml/qqmlextensionplugin.h>

Q_IMPORT_QML_PLUGIN(LaneUiPlugin)

using namespace Lane;

namespace
{

QString configFile()
{
    return QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)
        + QStringLiteral("/lane/config.json");
}

void removeConfig()
{
    QFile::remove(configFile());
}

} // namespace

class FirstRunTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void explicitSettingsConsumesFirstRun();
    void existingConfigIsNotFirstRun();
};

void FirstRunTest::explicitSettingsConsumesFirstRun()
{
    removeConfig();
    QVERIFY2(!QFile::exists(configFile()), "this case needs a missing config file");

    Controller fresh;
    QVERIFY2(fresh.firstRunPending(), "no config file means first run");

    fresh.openSettings();
    QVERIFY2(!fresh.firstRunPending(), "an explicit Settings open consumes first run");
    QWindow *window = fresh.settingsWindow();
    QVERIFY2(window, "Settings must actually load");
    QVERIFY(window->isVisible());

    fresh.maybeFirstRunSettings();
    QCOMPARE(fresh.settingsWindow(), window);
    QVERIFY2(!fresh.firstRunPending(), "the startup hook must not reopen first-run Settings");
}

void FirstRunTest::existingConfigIsNotFirstRun()
{
    const QString path = configFile();
    QVERIFY(QDir().mkpath(QFileInfo(path).absolutePath()));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QVERIFY(file.write("{}\n") >= 0);
    file.close();

    Controller again;
    QVERIFY2(!again.firstRunPending(), "an existing config is not a first run");
    again.maybeFirstRunSettings();
    QVERIFY2(again.settingsWindow() == nullptr, "startup hook must not open Settings when config exists");
}

int main(int argc, char **argv)
{
    QTemporaryDir tmp;
    if (!tmp.isValid()) {
        std::fprintf(stderr, "temp dir failed\n");
        return 2;
    }
    const QString run = tmp.path() + QStringLiteral("/run");
    QDir().mkpath(run);
    QFile::setPermissions(run, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
    qputenv("HOME", tmp.path().toUtf8());
    qputenv("XDG_CONFIG_HOME", (tmp.path() + QStringLiteral("/cfg")).toUtf8());
    qputenv("XDG_DATA_HOME", (tmp.path() + QStringLiteral("/data")).toUtf8());
    qputenv("XDG_DATA_DIRS", (tmp.path() + QStringLiteral("/share")).toUtf8());
    qputenv("XDG_RUNTIME_DIR", run.toUtf8());
    qputenv("QT_QPA_PLATFORM", "offscreen");
    qputenv("QT_QUICK_BACKEND", "software");
    qputenv("QT_QUICK_CONTROLS_STYLE", "Fusion");

    QApplication app(argc, argv);
    FirstRunTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_firstrun.moc"
