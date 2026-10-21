// Two regressions share this file: the picker key hints that only work
// while the filter field has focus must not linger once focus moves on
// (#83: number badge, comma/period ladder labels bound to
// fieldHintsActive; the Ctrl+C label follows copyHintActive since Ctrl+C
// stays live on a non-empty filter), and Alt+P must read a real targetId
// off the live ListView delegate (#57: a delegate that omits the
// required property leaves pickPrivate an empty string and it returns
// silently). Fails if the focus gate stays on after Tab, if any
// field-scoped label stays visible, or if the delegate drops targetId.

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
#include <QWindow>

#include <QQuickItem>
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

void seedTarget()
{
    // A discovered browser is unavailable in the sandbox, but a custom
    // target is a real row all the same: it gets a shortcut badge, and it
    // is enough for the picker to have something to hint.
    QJsonObject t;
    t[QStringLiteral("id")] = QStringLiteral("custom:test-browser");
    t[QStringLiteral("name")] = QStringLiteral("Test Browser");
    t[QStringLiteral("browserName")] = QStringLiteral("test");
    t[QStringLiteral("exec")] = QStringLiteral("/bin/true");
    t[QStringLiteral("args")] = QJsonArray::fromStringList({});
    t[QStringLiteral("icon")] = QStringLiteral("application-x-executable");
    t[QStringLiteral("kind")] = QStringLiteral("custom");

    QJsonObject root;
    root[QStringLiteral("customTargets")] = QJsonArray{t};

    const QString path = configFile();
    QVERIFY2(QDir().mkpath(QFileInfo(path).absolutePath()), "config dir");
    QFile file(path);
    QVERIFY2(file.open(QIODevice::WriteOnly | QIODevice::Truncate), "config file");
    file.write(QJsonDocument(root).toJson());
}

} // namespace

class PickerHintsTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void fieldHintsHideWithFocus();
    void altPReadsTargetIdFromDelegate();
};

void PickerHintsTest::fieldHintsHideWithFocus()
{
    seedTarget();

    Controller c;
    c.openUrl(QStringLiteral("https://example.com/page"), /*forcePicker=*/true);
    QWindow *window = c.pickerWindow();
    QVERIFY2(window, "the picker must actually load");
    QVERIFY(window->isVisible());
    QVERIFY2(c.pickerModel()->rowCount() > 0,
             "the seeded custom target must be a picker row");

    QObject *filter = window->findChild<QObject *>(QStringLiteral("pickerFilter"));
    QObject *copyHint = window->findChild<QObject *>(QStringLiteral("hintCopyLabel"));
    QObject *widenHint = window->findChild<QObject *>(QStringLiteral("widenHintLabel"));
    QObject *narrowHint = window->findChild<QObject *>(QStringLiteral("narrowHintLabel"));
    QVERIFY2(filter, "pickerFilter not found");
    QVERIFY2(copyHint, "hintCopyLabel not found");
    QVERIFY2(widenHint, "widenHintLabel not found");
    QVERIFY2(narrowHint, "narrowHintLabel not found");

    // Focused, empty filter: the shared gate and every footer field-hint
    // advertise a working key. (The row number badge shares the same gate;
    // ListView delegates are not reachable via QObject::findChild from the
    // window, so the gate is what the test can observe for that path.)
    QTRY_VERIFY_WITH_TIMEOUT(filter->property("activeFocus").toBool(), 5000);
    QTRY_VERIFY_WITH_TIMEOUT(window->property("fieldHintsActive").toBool(), 5000);
    QVERIFY(copyHint->property("visible").toBool());
    QVERIFY(widenHint->property("visible").toBool());
    QVERIFY(narrowHint->property("visible").toBool());

    // Typed text kills the digit/comma/period keys but Ctrl+C is handled
    // before the empty-text early return, so its hint must stay up while
    // the field is focused. The other field hints must drop with the
    // keys they advertise.
    filter->setProperty("text", QStringLiteral("ex"));
    QTRY_VERIFY_WITH_TIMEOUT(!window->property("fieldHintsActive").toBool(), 5000);
    QVERIFY2(window->property("copyHintActive").toBool(),
             "Ctrl+C still works on a non-empty filter; its hint must not hide");
    QVERIFY2(copyHint->property("visible").toBool(),
             "Ctrl+C label must stay while the key still works");
    QVERIFY2(!widenHint->property("visible").toBool(),
             "the . key is dead on a non-empty filter");
    QVERIFY2(!narrowHint->property("visible").toBool(),
             "the , key is dead on a non-empty filter");
    filter->setProperty("text", QString());

    // Tab-equivalent: focus moves to the Always checkbox. Digits, Ctrl+C
    // and comma/period are all bound to the field, so the gate and every
    // field-scoped label must drop.
    QObject *alwaysBox = window->findChild<QObject *>(QStringLiteral("alwaysBox"));
    QVERIFY2(alwaysBox, "the Always checkbox needs an objectName to take focus");
    QVERIFY(QMetaObject::invokeMethod(alwaysBox, "forceActiveFocus"));
    QTRY_VERIFY_WITH_TIMEOUT(!filter->property("activeFocus").toBool(), 5000);
    QVERIFY2(!window->property("fieldHintsActive").toBool(),
             "fieldHintsActive must clear once the filter loses focus");
    QVERIFY2(!copyHint->property("visible").toBool(),
             "the Ctrl+C label must not linger after Tab");
    QVERIFY2(!widenHint->property("visible").toBool(),
             "the . label must not linger after Tab");
    QVERIFY2(!narrowHint->property("visible").toBool(),
             "the , label must not linger after Tab");

    // Focus back on the field restores the hints: they describe live keys
    // again.
    QVERIFY(QMetaObject::invokeMethod(filter, "forceActiveFocus"));
    QTRY_VERIFY_WITH_TIMEOUT(window->property("fieldHintsActive").toBool(), 5000);
    QVERIFY(copyHint->property("visible").toBool());
    QVERIFY(widenHint->property("visible").toBool());
    QVERIFY(narrowHint->property("visible").toBool());
}

void PickerHintsTest::altPReadsTargetIdFromDelegate()
{
    // Regression for #57: Alt+P used list.currentItem.targetId, but the
    // delegate once omitted the required property, so pickPrivate got an
    // empty string and returned silently. The role and the property must
    // agree on a real id, and pickPrivate must not take the unknown-id path.
    seedTarget();

    Controller c;
    c.openUrl(QStringLiteral("https://example.com/page"), /*forcePicker=*/true);
    QWindow *window = c.pickerWindow();
    QVERIFY2(window, "the picker must actually load");
    QVERIFY2(c.pickerModel()->rowCount() > 0, "seeded row required");

    QObject *list = nullptr;
    QTRY_VERIFY_WITH_TIMEOUT(
        (list = window->findChild<QObject *>(QStringLiteral("pickerList"))) != nullptr, 5000);

    // Read the id through the same expression the Alt+P Shortcut uses:
    // list.currentItem.targetId (Picker.qml). itemAtIndex(0) alone would
    // pass even if the current row were the one dropping the property.
    QQuickItem *row = nullptr;
    QTRY_VERIFY_WITH_TIMEOUT(
        (QMetaObject::invokeMethod(list, "itemAtIndex", Q_RETURN_ARG(QQuickItem *, row), Q_ARG(int, 0)),
         row != nullptr),
        5000);

    QQuickItem *current = nullptr;
    QTRY_VERIFY_WITH_TIMEOUT(
        (current = list->property("currentItem").value<QQuickItem *>()) != nullptr,
        5000);

    const QString fromDelegate = current->property("targetId").toString();
    QVERIFY2(!fromDelegate.isEmpty(),
             "delegate must expose targetId; Alt+P reads it off currentItem");
    QCOMPARE(fromDelegate, c.pickerModel()->targetAt(0).id);
    QCOMPARE(current->property("targetId").toString(), row->property("targetId").toString());

    // Empty id must still surface the fail-closed notice (the silent path).
    c.pickPrivate(QString());
    QCOMPARE(c.property("pickerNotice").toString(), QStringLiteral("No private mode available"));
    c.clearPickerNotice();

    // A real id must not take that unknown-id branch. A custom /bin/true
    // target has no private counterpart, so the named notice is the proof
    // pickPrivate resolved the target.
    c.pickPrivate(fromDelegate);
    QVERIFY2(c.property("pickerNotice").toString().startsWith(QStringLiteral("No private mode for ")),
             qPrintable(c.property("pickerNotice").toString()));
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
    PickerHintsTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_pickerhints.moc"
