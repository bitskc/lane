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
        QVERIFY(text.contains(QStringLiteral("X-KDE-Shortcuts=Ctrl+Alt+S")));

        QVERIFY(text.contains(QStringLiteral("[Desktop Action Rescan]")));
        QVERIFY(text.contains(QStringLiteral("Exec=@LANE_BIN@ --rediscover")));
    }

    void shortcutsParseCleanlyInKGlobalAccel()
    {
        // kglobalacceld parses X-KDE-Shortcuts by replacing ',' with '\t',
        // splitting on '\t', and converting each part with QKeySequence::fromString.
        // A shortcut containing a literal comma (e.g. Ctrl+Alt+,) gets split and
        // results in an invalid/empty key sequence. Verify every shortcut in the
        // desktop file produces valid, non-empty QKeySequences.
        QFile file(QStringLiteral(LANE_SOURCE_ROOT) + QStringLiteral("/data/app.lane.Lane.desktop.in"));
        QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
        const QString text = QString::fromUtf8(file.readAll());

        const QStringList lines = text.split(QLatin1Char('\n'));
        int shortcutCount = 0;
        for (const QString &line : lines) {
            if (line.startsWith(QLatin1String("X-KDE-Shortcuts="))) {
                ++shortcutCount;
                const QString val = line.mid(16);
                QString s = val;
                s.replace(QLatin1Char(','), QLatin1Char('\t'));
                const QStringList parts = s.split(QLatin1Char('\t'), Qt::SkipEmptyParts);
                QVERIFY2(!parts.isEmpty(), "shortcut entry must not be empty");
                for (const QString &part : parts) {
                    const QKeySequence seq = QKeySequence::fromString(part);
                    QVERIFY2(!seq.isEmpty(), qPrintable(QStringLiteral("Shortcut failed to parse: %1 in line: %2").arg(part, line)));
                    QVERIFY2(!seq.toString().isEmpty(), qPrintable(QStringLiteral("Shortcut resolved to null key: %1").arg(part)));
                }
            }
        }
        QCOMPARE(shortcutCount, 4); // main entry + 3 actions with shortcuts
    }
};

QTEST_MAIN(KdeActionsTest)
#include "test_kde_actions.moc"
