#include <QDBusConnection>
#include <QDBusMessage>
#include <QFile>
#include <QKeySequence>
#include <QProcess>
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
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("unknown D-Bus action")));
        QCOMPARE(argsForAction(QStringLiteral("")),
                 QStringList({QStringLiteral("lane")}));
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("unknown D-Bus action")));
        QCOMPARE(argsForAction(QStringLiteral("NonExistent")),
                 QStringList({QStringLiteral("lane")}));
    }

    void activateActionRoutesMappedArgvOverDBus()
    {
        // End-to-end: a real ActivateAction call on a private session bus
        // must deliver the mapped argv to handleArgs. Without this, a
        // revert of ActivateAction to m_handleArgs({}) leaves every
        // other test green.
        QProcess daemon;
        daemon.start(QStringLiteral("dbus-daemon"),
                     {QStringLiteral("--session"), QStringLiteral("--print-address"), QStringLiteral("--nofork")});
        if (!daemon.waitForStarted()) {
            QSKIP("dbus-daemon not available; cannot run D-Bus round-trip test");
        }
        QVERIFY(daemon.waitForReadyRead(5000));
        const QString address = QString::fromLatin1(daemon.readLine()).trimmed();
        QVERIFY(!address.isEmpty());

        QDBusConnection bus = QDBusConnection::connectToBus(address, QStringLiteral("kdetest"));
        QVERIFY(bus.isConnected());
        qputenv("DBUS_SESSION_BUS_ADDRESS", address.toUtf8());
        QDBusConnection::disconnectFromBus(QStringLiteral("kdetest"));

        QStringList seen;
        QVERIFY(registerActivation(
            [&seen](const QStringList &a) { seen << a.join(QLatin1Char(' ')); },
            [](const QString &) {}));

        auto callAction = [&seen](const QString &name) {
            QDBusMessage m = QDBusMessage::createMethodCall(
                QStringLiteral("app.lane.Lane"), QStringLiteral("/app/lane/Lane"),
                QStringLiteral("org.freedesktop.Application"),
                QStringLiteral("ActivateAction"));
            m << name << QVariantList() << QVariantMap();
            QDBusConnection::sessionBus().call(m);
            qApp->processEvents();
        };

        callAction(QStringLiteral("Clipboard"));
        callAction(QStringLiteral("Settings"));

        // Unknown action degrades to a bare activation.
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("unknown D-Bus action")));
        callAction(QStringLiteral("Bogus"));

        QCOMPARE(seen, QStringList({QStringLiteral("lane --clipboard"),
                                    QStringLiteral("lane --settings"),
                                    QStringLiteral("lane")}));

        daemon.terminate();
        daemon.waitForFinished(2000);
    }

    void desktopFileDeclaresAllActionsAndShortcuts()
    {
        // Parse the template desktop file into groups and assert each
        // [Desktop Action X] section owns the expected Exec and
        // X-KDE-Shortcuts value. contains()-style assertions cannot catch
        // a value swapped between groups.
        QFile file(QStringLiteral(LANE_SOURCE_ROOT) + QStringLiteral("/data/app.lane.Lane.desktop.in"));
        QVERIFY2(file.open(QIODevice::ReadOnly | QIODevice::Text), "failed to open app.lane.Lane.desktop.in");
        const QString text = QString::fromUtf8(file.readAll());

        QHash<QString, QHash<QString, QString>> groups;
        QString current;
        for (const QString &line : text.split(QLatin1Char('\n'))) {
            if (line.startsWith(QLatin1Char('[')) && line.endsWith(QLatin1Char(']'))) {
                current = line.mid(1, line.size() - 2);
            } else {
                const int eq = line.indexOf(QLatin1Char('='));
                if (!current.isEmpty() && eq > 0) {
                    groups[current][line.left(eq)] = line.mid(eq + 1);
                }
            }
        }

        QVERIFY(groups.value(QStringLiteral("Desktop Entry"))
                    .value(QStringLiteral("Actions"))
                    .contains(QStringLiteral("Clipboard;ClipboardPicker;Settings;Rescan;")));

        const QHash<QString, QString> clipboard = groups.value(QStringLiteral("Desktop Action Clipboard"));
        QCOMPARE(clipboard.value(QStringLiteral("Exec")), QStringLiteral("@LANE_BIN@ --clipboard"));
        QCOMPARE(clipboard.value(QStringLiteral("X-KDE-Shortcuts")), QStringLiteral("Ctrl+Alt+V"));

        const QHash<QString, QString> picker = groups.value(QStringLiteral("Desktop Action ClipboardPicker"));
        QCOMPARE(picker.value(QStringLiteral("Exec")), QStringLiteral("@LANE_BIN@ --clipboard -p"));
        QCOMPARE(picker.value(QStringLiteral("X-KDE-Shortcuts")), QStringLiteral("Ctrl+Alt+Shift+V"));

        const QHash<QString, QString> settings = groups.value(QStringLiteral("Desktop Action Settings"));
        QCOMPARE(settings.value(QStringLiteral("Exec")), QStringLiteral("@LANE_BIN@ --settings"));
        QCOMPARE(settings.value(QStringLiteral("X-KDE-Shortcuts")), QStringLiteral("Ctrl+Alt+S"));

        const QHash<QString, QString> rescan = groups.value(QStringLiteral("Desktop Action Rescan"));
        QCOMPARE(rescan.value(QStringLiteral("Exec")), QStringLiteral("@LANE_BIN@ --rediscover"));
        QVERIFY(!rescan.contains(QStringLiteral("X-KDE-Shortcuts")));
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

        constexpr QLatin1String kShortcutKey{"X-KDE-Shortcuts="};
        const QStringList lines = text.split(QLatin1Char('\n'));
        int shortcutCount = 0;
        for (const QString &line : lines) {
            if (line.startsWith(kShortcutKey)) {
                ++shortcutCount;
                QString s = line.mid(kShortcutKey.size());
                s.replace(QLatin1Char(','), QLatin1Char('\t'));
                // KeepEmptyParts: a trailing/doubled comma must fail, not pass
                // silently. Each segment must parse to a real key.
                const QStringList parts = s.split(QLatin1Char('\t'), Qt::KeepEmptyParts);
                QVERIFY2(!parts.isEmpty(), "shortcut entry must not be empty");
                for (const QString &part : parts) {
                    QVERIFY2(!part.isEmpty(), qPrintable(QStringLiteral("Empty shortcut segment in line: %1").arg(line)));
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
