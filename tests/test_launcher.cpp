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

    void expandsUrlEncodedPlaceholder()
    {
        Target t;
        t.args = {QStringLiteral("--profile"), QStringLiteral("/prof"), QStringLiteral("--new-tab"),
                   QStringLiteral("ext+container:name=Work&url=$urlEncoded")};
        const auto args = expandArgs(t, QStringLiteral("https://example.com/a b?x=1&y=2"));
        QCOMPARE(args.size(), 4);
        QCOMPARE(args.last(),
                 QStringLiteral("ext+container:name=Work&url=https%3A%2F%2Fexample.com%2Fa%20b%3Fx%3D1%26y%3D2"));
    }

    void urlEncodedPlaceholderEscapesUnicode()
    {
        Target t;
        t.args = {QStringLiteral("--new-tab"), QStringLiteral("ext+container:name=Dev&url=$urlEncoded")};
        const auto args = expandArgs(t, QString::fromUtf8("https://example.com/caf\xc3\xa9"));
        QVERIFY(!args.last().contains(QString::fromUtf8("caf\xc3\xa9")));
        QVERIFY(args.last().contains(QStringLiteral("caf%C3%A9")));
    }

    void urlEncodedPlaceholderNeverInjectsNewline()
    {
        Target t;
        t.args = {QStringLiteral("ext+container:name=Dev&url=$urlEncoded")};
        // Percent-encoding keeps the substitution inside a single argv
        // token rather than letting a raw newline slip through.
        const auto args = expandArgs(t, QStringLiteral("https://example.com/\nrm -rf"));
        QCOMPARE(args.size(), 1);
        QVERIFY(!args.first().contains(QLatin1Char('\n')));
        QVERIFY(!args.first().contains(QLatin1Char(' ')));
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
