#include <QFile>
#include <QStringList>
#include <QTest>

#include "../src/app/DBusActivation.h"

using namespace Lane;

class KdeActionsTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void mapsDesktopActionsToArguments()
    {
        QCOMPARE(argsForAction(QStringLiteral("Clipboard")),
                 QStringList({QStringLiteral("lane"), QStringLiteral("--clipboard")}));

        QCOMPARE(argsForAction(QStringLiteral("ClipboardPicker")),
                 QStringList({QStringLiteral("lane"), QStringLiteral("--clipboard"), QStringLiteral("-p")}));

        QCOMPARE(argsForAction(QStringLiteral("Settings")),
                 QStringList({QStringLiteral("lane"), QStringLiteral("--settings")}));

        QCOMPARE(argsForAction(QStringLiteral("Rescan")),
                 QStringList({QStringLiteral("lane"), QStringLiteral("--rediscover")}));

        // Unknown or empty action returns bare launch
        QCOMPARE(argsForAction(QStringLiteral("")),
                 QStringList({QStringLiteral("lane")}));
        QCOMPARE(argsForAction(QStringLiteral("NonExistent")),
                 QStringList({QStringLiteral("lane")}));
    }

    void desktopFileDeclaresAllActionsAndShortcuts()
    {
        // Read the template desktop file from data/
        QFile file(QStringLiteral(LANE_SOURCE_ROOT) + QStringLiteral("/data/app.lane.Lane.desktop.in"));
        QVERIFY2(file.open(QIODevice::ReadOnly | QIODevice::Text), "failed to open app.lane.Lane.desktop.in");
        const QString text = QString::fromUtf8(file.readAll());

        QVERIFY(text.contains(QStringLiteral("Actions=Clipboard;ClipboardPicker;Settings;Rescan;")));

        QVERIFY(text.contains(QStringLiteral("[Desktop Action Clipboard]")));
        QVERIFY(text.contains(QStringLiteral("Exec=@LANE_BIN@ --clipboard")));
        QVERIFY(text.contains(QStringLiteral("X-KDE-Shortcuts=Ctrl+Alt+V")));

        QVERIFY(text.contains(QStringLiteral("[Desktop Action ClipboardPicker]")));
        QVERIFY(text.contains(QStringLiteral("Exec=@LANE_BIN@ --clipboard -p")));
        QVERIFY(text.contains(QStringLiteral("X-KDE-Shortcuts=Ctrl+Alt+Shift+V")));

        QVERIFY(text.contains(QStringLiteral("[Desktop Action Settings]")));
        QVERIFY(text.contains(QStringLiteral("Exec=@LANE_BIN@ --settings")));
        QVERIFY(text.contains(QStringLiteral("X-KDE-Shortcuts=Ctrl+Alt+,")));

        QVERIFY(text.contains(QStringLiteral("[Desktop Action Rescan]")));
        QVERIFY(text.contains(QStringLiteral("Exec=@LANE_BIN@ --rediscover")));
    }
};

QTEST_MAIN(KdeActionsTest)
#include "test_kde_actions.moc"
