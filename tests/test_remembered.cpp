// Remembered hosts that point at a missing target must survive reload().
// Discovery false negatives that last for the process lifetime (bare
// systemd unit missing XDG_DATA_DIRS, Flatpak export mid-update) look
// identical to an uninstall, so any auto-prune keyed on consecutive
// reloads still deletes the user's routing decision. Only the explicit
// Clear-dead action may prune. Fails if three reload() passes drop the
// dangling host.

#include "app/Controller.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

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

void seedConfig()
{
    QJsonObject custom;
    custom[QStringLiteral("id")] = QStringLiteral("custom:alive");
    custom[QStringLiteral("name")] = QStringLiteral("Alive");
    custom[QStringLiteral("exec")] = QStringLiteral("/bin/true");
    custom[QStringLiteral("args")] = QJsonArray::fromStringList({});
    custom[QStringLiteral("icon")] = QStringLiteral("application-x-executable");
    custom[QStringLiteral("kind")] = QStringLiteral("custom");

    QJsonObject remembered;
    remembered[QStringLiteral("old-site.example")] = QStringLiteral("browser:gone:uninstalled");
    remembered[QStringLiteral("live.example")] = QStringLiteral("custom:alive");

    QJsonObject root;
    root[QStringLiteral("customTargets")] = QJsonArray{custom};
    root[QStringLiteral("remembered")] = remembered;

    const QString path = configFile();
    QVERIFY2(QDir().mkpath(QFileInfo(path).absolutePath()), "config dir");
    QFile file(path);
    QVERIFY2(file.open(QIODevice::WriteOnly | QIODevice::Truncate), "config file");
    file.write(QJsonDocument(root).toJson());
}

} // namespace

class RememberedTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void reloadDoesNotAutoPruneDanglingHosts();
};

void RememberedTest::reloadDoesNotAutoPruneDanglingHosts()
{
    seedConfig();

    Controller c;
    QCOMPARE(c.rememberedTarget(QStringLiteral("old-site.example")),
             QStringLiteral("browser:gone:uninstalled"));
    QCOMPARE(c.danglingRememberedHosts(), QStringList{QStringLiteral("old-site.example")});

    // Three rediscovers (each calls reload): the old grace threshold. A
    // persistent discovery miss inside one process must still leave the
    // host alone.
    c.rediscover();
    c.rediscover();
    c.rediscover();
    QCOMPARE(c.rememberedTarget(QStringLiteral("old-site.example")),
             QStringLiteral("browser:gone:uninstalled"));
    QCOMPARE(c.rememberedTarget(QStringLiteral("live.example")),
             QStringLiteral("custom:alive"));

    // Only the explicit action may prune.
    c.clearDeadRemembered();
    QVERIFY(c.rememberedTarget(QStringLiteral("old-site.example")).isEmpty());
    QCOMPARE(c.rememberedTarget(QStringLiteral("live.example")),
             QStringLiteral("custom:alive"));
    QVERIFY(c.danglingRememberedHosts().isEmpty());
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
    RememberedTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_remembered.moc"
