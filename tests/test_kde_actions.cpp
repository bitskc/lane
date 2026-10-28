#include <QDBusConnection>
#include <QDBusMessage>
#include <QFile>
#include <QKeySequence>
#include <QStringList>
#include <QTest>

#include <cstdio>
#include <cstdlib>
#include <csignal>
#include <sys/types.h>
#include <unistd.h>

#include "../src/app/DBusActivation.h"

using namespace Lane;

namespace
{

// The D-Bus test needs a session bus whose address is in the environment
// BEFORE QCoreApplication/QApplication exists: QDBusConnection::sessionBus()
// caches its connection, so touching it before qputenv would attach every
// later call (including registerActivation's name claim) to the real user
// bus. Spawn a private dbus-daemon in a global constructor — it runs before
// main — and kill it in the destructor.
struct PrivateBus {
    PrivateBus()
    {
        // --print-pid emits "<pid>" on the line after the address; both
        // come from the daemon's stdout before it detaches.
        FILE *fp = popen("dbus-daemon --session --fork --print-address --print-pid", "r");
        if (!fp) {
            return;
        }
        char buf[512];
        if (!fgets(buf, sizeof(buf), fp)) {
            pclose(fp);
            return;
        }
        address = QString::fromLatin1(buf).trimmed();
        // Set the env var as soon as the address parses. The guard in the
        // test checks `address` is non-empty; without an immediate qputenv
        // a partial read below would still run registerActivation on the
        // real user session bus and claim its well-known name.
        if (!address.isEmpty()) {
            qputenv("DBUS_SESSION_BUS_ADDRESS", address.toUtf8());
        }
        if (!fgets(buf, sizeof(buf), fp)) {
            pclose(fp);
            return;
        }
        pid = QString::fromLatin1(buf).trimmed().toLong();
        pclose(fp);
    }
    ~PrivateBus()
    {
        if (pid > 0) {
            kill(static_cast<pid_t>(pid), SIGTERM);
        }
    }
    QString address;
    long pid = -1;
};

PrivateBus g_bus;

void callAction(const QString &name)
{
    QDBusMessage m = QDBusMessage::createMethodCall(
        QStringLiteral("app.lane.Lane"), QStringLiteral("/app/lane/Lane"),
        QStringLiteral("org.freedesktop.Application"),
        QStringLiteral("ActivateAction"));
    m << name << QVariantList() << QVariantMap();
    QDBusConnection::sessionBus().call(m);
    qApp->processEvents();
}

} // namespace

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
        // End-to-end: a real ActivateAction call on the private session bus
        // must deliver the mapped argv to handleArgs. Without this, a
        // revert of ActivateAction to m_handleArgs({}) leaves every
        // other test green. The bus was spawned before QApplication, so
        // sessionBus() can only ever mean our private daemon.
        QVERIFY2(!g_bus.address.isEmpty(), "dbus-daemon not available; cannot run D-Bus round-trip test");

        QStringList seen;
        QVERIFY(registerActivation(
            [&seen](const QStringList &a) { seen << a.join(QLatin1Char(' ')); },
            [](const QString &) {}));

        callAction(QStringLiteral("Clipboard"));
        callAction(QStringLiteral("Settings"));

        // Unknown action degrades to a bare activation.
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("unknown D-Bus action")));
        callAction(QStringLiteral("Bogus"));

        QCOMPARE(seen, QStringList({QStringLiteral("lane --clipboard"),
                                    QStringLiteral("lane --settings"),
                                    QStringLiteral("lane")}));
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

        // Exact list, not a substring check: a fifth action appended to
        // Actions= must fail here until the test is updated.
        QCOMPARE(groups.value(QStringLiteral("Desktop Entry"))
                     .value(QStringLiteral("Actions")),
                 QStringLiteral("Clipboard;ClipboardPicker;Settings;Rescan;"));

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

    void desktopExecArgsMatchActivateActionMapping()
    {
        // The .desktop Exec path and the D-Bus ActivateAction path must
        // deliver the same argv for every declared action. argsForAction's
        // comment claims this test enforces the sync; only a real
        // cross-check does that. For each action group, strip the
        // @LANE_BIN@ prefix from Exec and compare with argsForAction
        // minus its argv[0].
        QFile file(QStringLiteral(LANE_SOURCE_ROOT) + QStringLiteral("/data/app.lane.Lane.desktop.in"));
        QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
        const QString text = QString::fromUtf8(file.readAll());

        // The name list comes from Actions= itself, not a copy of it:
        // adding an action to the .desktop without an argsForAction
        // mapping must fail here (see issue #120).
        const QString actionsLine = [&text]() {
            for (const QString &line : text.split(QLatin1Char('\n'))) {
                if (line.startsWith(QLatin1String("Actions="))) {
                    return line.mid(8);
                }
            }
            return QString();
        }();
        const QStringList actionNames = actionsLine.split(QLatin1Char(';'), Qt::SkipEmptyParts);
        QCOMPARE(actionNames, QStringList({QStringLiteral("Clipboard"),
                                           QStringLiteral("ClipboardPicker"),
                                           QStringLiteral("Settings"),
                                           QStringLiteral("Rescan")}));

        for (const QString &name : actionNames) {
            const QString group = QStringLiteral("[Desktop Action ") + name + QLatin1Char(']');
            const int gstart = text.indexOf(group);
            QVERIFY2(gstart >= 0, qPrintable(QStringLiteral("missing group %1").arg(group)));
            const int gend = text.indexOf(QLatin1Char('['), gstart + group.size());
            const QString body = (gend < 0) ? text.mid(gstart) : text.mid(gstart, gend - gstart);
            QString exec;
            for (const QString &line : body.split(QLatin1Char('\n'))) {
                if (line.startsWith(QLatin1String("Exec="))) {
                    exec = line.mid(5).trimmed();
                    break;
                }
            }
            QVERIFY2(!exec.isEmpty(), qPrintable(QStringLiteral("no Exec in %1").arg(group)));
            QVERIFY2(exec.startsWith(QLatin1String("@LANE_BIN@")),
                     qPrintable(QStringLiteral("Exec does not start with @LANE_BIN@ in %1").arg(group)));
            const QStringList execArgs = exec.mid(QLatin1String("@LANE_BIN@").size()).trimmed().split(QLatin1Char(' '), Qt::SkipEmptyParts);

            QStringList mapped = argsForAction(name);
            QVERIFY(!mapped.isEmpty());
            mapped.removeFirst(); // drop argv[0]
            QCOMPARE(mapped, execArgs);
        }
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
