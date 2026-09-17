#include "core/launcher.h"

#include <QFile>
#include <QFileDevice>
#include <QTemporaryDir>
#include <QTest>

using namespace Lane;

// Writes a small executable script that dumps its own environment to
// outPath. Invoked directly as target.exec (not via /bin/sh -c), so it is
// not one of the interpreter basenames isBlockedInterpreter() refuses, and
// this exercises the exact QProcess::startDetached() path launchTarget()
// uses for a real destination.
static QString writeDumpEnvScript(const QTemporaryDir &dir, const QString &name, const QString &outPath)
{
    const QString path = dir.filePath(name);
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return {};
    }
    f.write(QStringLiteral("#!/bin/sh\nenv > '%1'\n").arg(outPath).toUtf8());
    f.close();
    QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
    return path;
}

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
        QVERIFY(!error.isEmpty());
        error.clear();
        QVERIFY(!parseCustomCommand(QStringLiteral("python3 -c 'print(1)'"), &t, &error));
        QVERIFY(!error.isEmpty());
    }

    void rejectedCustomCommandErrorsAreDistinctAndActionable()
    {
        // Controller::addCustomTarget surfaces this string verbatim in the
        // "Add custom app" UI, so it must say something a user can act on,
        // and it must differ per failure reason rather than one generic string.
        Target t;
        QString emptyError;
        QVERIFY(!parseCustomCommand(QStringLiteral("   "), &t, &emptyError));
        QVERIFY(!emptyError.isEmpty());

        QString missingError;
        QVERIFY(!parseCustomCommand(QStringLiteral("this-binary-does-not-exist-anywhere --flag"), &t, &missingError));
        QVERIFY(!missingError.isEmpty());

        QString shellError;
        QVERIFY(!parseCustomCommand(QStringLiteral("bash -c 'rm -rf /'"), &t, &shellError));
        QVERIFY(!shellError.isEmpty());

        QVERIFY(emptyError != missingError);
        QVERIFY(missingError != shellError);
        QVERIFY(emptyError != shellError);
    }

    void refusesFileUrlLaunch()
    {
        Target t;
        t.exec = QStringLiteral("/usr/bin/true");
        t.args = {QStringLiteral("$url")};
        QVERIFY(!launchTarget(t, QStringLiteral("file:///etc/passwd")));
        QVERIFY(!launchTarget(t, QStringLiteral("javascript:alert(1)")));
    }

    void launchTargetRejectsBlockedInterpreterEvenWithoutParsing()
    {
        // Built directly on a Target, bypassing parseCustomCommand entirely,
        // e.g. as if loaded straight from a hand-edited config.json.
        Target t;
        t.kind = Kind::Custom;
        t.exec = QStringLiteral("/bin/sh");
        t.args = {QStringLiteral("-c"), QStringLiteral("echo pwned"), QStringLiteral("$url")};
        QVERIFY(!launchTarget(t, QStringLiteral("https://example.com")));
    }

    void launchTargetRejectsSymlinkToBlockedInterpreter()
    {
        // Proven bypass: exec's own basename is innocuous, but it is a
        // symlink to a blocked interpreter. isBlockedInterpreter() alone
        // (a plain basename check on t.exec) sails straight through this;
        // only walking the symlink's actual target catches it.
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString linkPath = dir.filePath(QStringLiteral("totally-a-browser"));
        QVERIFY(QFile::link(QStringLiteral("/bin/sh"), linkPath));

        Target t;
        t.kind = Kind::Custom;
        t.exec = linkPath;
        t.args = {QStringLiteral("-c"), QStringLiteral("echo pwned"), QStringLiteral("$url")};
        QVERIFY(!launchTarget(t, QStringLiteral("https://example.com")));
    }

    void launchTargetRejectsEnvReExecWrapper()
    {
        // Second proven bypass: exec itself is never a blocked
        // interpreter, only a wrapper (env) that re-execs one named in
        // its own args. Closed by blocking the wrapper binary itself
        // rather than trying to parse argv; see blockedInterpreters() in
        // launcher.cpp for the accepted residual risk.
        Target t;
        t.kind = Kind::Custom;
        t.exec = QStringLiteral("/usr/bin/env");
        t.args = {QStringLiteral("bash"), QStringLiteral("-c"), QStringLiteral("echo pwned"), QStringLiteral("$url")};
        QVERIFY(!launchTarget(t, QStringLiteral("https://example.com")));
    }

    // launchTarget() spawns via the static, argument-only
    // QProcess::startDetached(program, arguments) overload, which has no
    // QProcessEnvironment parameter and always forks Lane's own live
    // environment. Proves the actual claim (env lands in the child) rather
    // than assuming it from Qt's setProcessEnvironment() caveat, by reading
    // back what a real spawned child saw. The script is invoked directly
    // (not via /bin/sh, which isBlockedInterpreter() refuses) so this
    // exercises exactly the same launchTarget() path a real browser target
    // would take.
    void launchTargetSetsActivationTokenForDetachedSpawn()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString outPath = dir.filePath(QStringLiteral("env-with-token.txt"));
        const QString script = writeDumpEnvScript(dir, QStringLiteral("dump-with-token"), outPath);
        QVERIFY(!script.isEmpty());

        Target t;
        t.kind = Kind::Custom;
        t.exec = script;
        QVERIFY(launchTarget(t, QStringLiteral("https://example.com"), QStringLiteral("proof-token-xyz")));

        QTRY_VERIFY_WITH_TIMEOUT(QFile::exists(outPath), 2000);
        QFile f(outPath);
        QVERIFY(f.open(QIODevice::ReadOnly));
        QVERIFY(f.readAll().contains("proof-token-xyz"));
    }

    // No token supplied: the child's environment must be exactly as before,
    // not carrying a stray XDG_ACTIVATION_TOKEN from some earlier launch.
    void launchTargetLeavesEnvironmentUntouchedWithoutToken()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString outPath = dir.filePath(QStringLiteral("env-without-token.txt"));
        const QString script = writeDumpEnvScript(dir, QStringLiteral("dump-without-token"), outPath);
        QVERIFY(!script.isEmpty());

        Target t;
        t.kind = Kind::Custom;
        t.exec = script;
        QVERIFY(launchTarget(t, QStringLiteral("https://example.com")));

        QTRY_VERIFY_WITH_TIMEOUT(QFile::exists(outPath), 2000);
        QFile f(outPath);
        QVERIFY(f.open(QIODevice::ReadOnly));
        QVERIFY(!f.readAll().contains("XDG_ACTIVATION_TOKEN"));
    }

    // A fake `flatpak` executable: basename "flatpak" is legitimately not
    // in blockedInterpreters() (real browser desktop entries launch
    // through `flatpak run`), so its argv needs its own inspection.
    void launchTargetRejectsFlatpakCommandShell()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString flatpak = writeDumpEnvScript(dir, QStringLiteral("flatpak"),
                                                   dir.filePath(QStringLiteral("out.txt")));
        QVERIFY(!flatpak.isEmpty());

        Target t;
        t.kind = Kind::Custom;
        t.exec = flatpak;
        t.args = {QStringLiteral("run"), QStringLiteral("--command=sh"),
                  QStringLiteral("app.zen_browser.zen"), QStringLiteral("$url")};
        QVERIFY(!launchTarget(t, QStringLiteral("https://example.com")));

        // Same bypass with --command as a separate token.
        t.args = {QStringLiteral("run"), QStringLiteral("--command"), QStringLiteral("bash"),
                  QStringLiteral("app.zen_browser.zen"), QStringLiteral("$url")};
        QVERIFY(!launchTarget(t, QStringLiteral("https://example.com")));
    }

    void launchTargetRejectsFlatpakRunWithoutAppId()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString flatpak = writeDumpEnvScript(dir, QStringLiteral("flatpak"),
                                                   dir.filePath(QStringLiteral("out.txt")));
        QVERIFY(!flatpak.isEmpty());

        Target t;
        t.kind = Kind::Custom;
        t.exec = flatpak;
        t.args = {QStringLiteral("run"), QStringLiteral("--command=zen"), QStringLiteral("$url")};
        QVERIFY(!launchTarget(t, QStringLiteral("https://example.com")));
    }

    void launchTargetAllowsLegitFlatpakRun()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString outPath = dir.filePath(QStringLiteral("flatpak-args.txt"));
        const QString script = dir.filePath(QStringLiteral("flatpak"));
        QFile f(script);
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
        f.write(QStringLiteral("#!/bin/sh\nprintf '%s\\n' \"$@\" > '%1'\n").arg(outPath).toUtf8());
        f.close();
        QFile::setPermissions(script, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);

        Target t;
        t.kind = Kind::Custom;
        t.exec = script;
        t.args = {QStringLiteral("run"), QStringLiteral("--branch=stable"), QStringLiteral("--command=zen"),
                  QStringLiteral("app.zen_browser.zen"), QStringLiteral("--new-tab"), QStringLiteral("$url")};
        QVERIFY(launchTarget(t, QStringLiteral("https://example.com")));

        QTRY_VERIFY_WITH_TIMEOUT(QFile::exists(outPath), 2000);
        QFile out(outPath);
        QVERIFY(out.open(QIODevice::ReadOnly));
        QVERIFY(out.readAll().contains("app.zen_browser.zen"));
    }
};

QTEST_MAIN(LauncherTest)
#include "test_launcher.moc"
