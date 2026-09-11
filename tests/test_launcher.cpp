#include "core/launcher.h"

#include <QTest>

using namespace Tern;

class LauncherTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void expandsUrlPlaceholder()
    {
        Target t;
        t.args = {QStringLiteral("-P"), QStringLiteral("Work"), QStringLiteral("--new-tab"), QStringLiteral("$url")};
        const auto args = expandArgs(t, QStringLiteral("https://example.com"));
        QCOMPARE(args.last(), QStringLiteral("https://example.com"));
        QCOMPARE(args.size(), 4);
    }

    void appendsWhenMissing()
    {
        Target t;
        t.args = {QStringLiteral("--incognito")};
        const auto args = expandArgs(t, QStringLiteral("https://x.test"));
        QCOMPARE(args, QStringList({QStringLiteral("--incognito"), QStringLiteral("https://x.test")}));
    }

    void pwaUrlFlag()
    {
        Target t;
        t.kind = Kind::Pwa;
        t.args = {QStringLiteral("site"), QStringLiteral("launch"), QStringLiteral("ULID"), QStringLiteral("--url"), QStringLiteral("$url")};
        const auto args = expandArgs(t, QStringLiteral("https://claude.ai/new"));
        QCOMPARE(args.last(), QStringLiteral("https://claude.ai/new"));
    }

    void rejectsShellCustomCommand()
    {
        Target t;
        QString error;
        QVERIFY(!parseCustomCommand(QStringLiteral("bash -c 'rm -rf /'"), &t, &error));
        QVERIFY(!parseCustomCommand(QStringLiteral("python3 -c 'print(1)'"), &t, &error));
    }

    void refusesFileUrlLaunch()
    {
        Target t;
        t.exec = QStringLiteral("/usr/bin/true");
        t.args = {QStringLiteral("$url")};
        QVERIFY(!launchTarget(t, QStringLiteral("file:///etc/passwd")));
        QVERIFY(!launchTarget(t, QStringLiteral("javascript:alert(1)")));
    }
};

QTEST_MAIN(LauncherTest)
#include "test_launcher.moc"
