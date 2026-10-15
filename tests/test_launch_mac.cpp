#include "core/launcher.h"
#include "core/urlutil.h"

#include <QFile>
#include <QFileDevice>
#include <QTemporaryDir>
#include <QTest>

using namespace Lane;

// Writes an executable script that records its argv, one argument per
// line, to outPath. Invoked directly as target.exec (never via /bin/sh,
// which the blocklist refuses), so it exercises the same
// QProcess::startDetached() path a real bundle executable takes.
static QString writeDumpArgvScript(const QTemporaryDir &dir, const QString &name, const QString &outPath)
{
    const QString path = dir.filePath(name);
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return {};
    }
    // QString::arg() only consumes %<digit> markers, so the %s below is
    // passed through to the shell untouched. Write-then-rename keeps a
    // waiter polling exists() from reading partial content (same race as
    // in test_launcher.cpp).
    f.write(QStringLiteral("#!/bin/sh\nprintf '%s\\n' \"$@\" > '%1.tmp' && mv '%1.tmp' '%1'\n")
                .arg(outPath)
                .toUtf8());
    f.close();
    QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
    return path;
}

static QString writePlainExec(const QTemporaryDir &dir, const QString &name)
{
    return writeDumpArgvScript(dir, name, dir.filePath(QStringLiteral("unused-") + name));
}

// Reads back a file once it exists (the detached child may need a moment).
// QTRY_* only works in void functions, so this polls manually.
static QByteArray readWhenReady(const QString &path)
{
    for (int i = 0; i < 100 && !QFile::exists(path); ++i) {
        QTest::qWait(20);
    }
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        return {};
    }
    return f.readAll();
}

class LaunchMacTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    // action:email on macOS: /usr/bin/open with a mailto: URL whose body
    // is the whole sanitized URL percent-encoded.
    void macOpenArgsEmailBuildsMailtoBody()
    {
        Target t;
        t.kind = Kind::Action;
        t.engine = Engine::Action;
        t.id = QStringLiteral("action:email");
        t.exec = QStringLiteral("/usr/bin/open");

        const QString open = sanitizedOpenUrl(QStringLiteral("https://example.com/a?b=1"));
        const auto args = macFixedOpenArgs(t, open);
        QVERIFY(args.has_value());
        QCOMPARE(args->size(), 1);
        QCOMPARE(args->first(), QStringLiteral("mailto:?body=https%3A%2F%2Fexample.com%2Fa%3Fb%3D1"));
    }

    // Header injection: a URL carrying &to=&cc=&subject= must still
    // produce exactly one body parameter and nothing else.
    void macOpenArgsEmailCannotInjectHeaders()
    {
        Target t;
        t.kind = Kind::Action;
        t.engine = Engine::Action;
        t.id = QStringLiteral("action:email");

        const QString raw = QStringLiteral("https://x.test/?a=1&to=evil@x.test&cc=a@b.c&subject=hi");
        const QString open = sanitizedOpenUrl(raw);
        QVERIFY(!open.isEmpty());
        const auto args = macFixedOpenArgs(t, open);
        QVERIFY(args.has_value());
        QCOMPARE(args->size(), 1);

        // Verify the invariant on the raw string rather than through
        // QUrlQuery: Qt partially decodes mailto query items, which would
        // mask (or fake) an injection. After `mailto:?body=` there may be
        // no unencoded &, = or ? -- those are the separators an attacker
        // would need to add a second parameter.
        const QString arg = args->first();
        QVERIFY(arg.startsWith(QStringLiteral("mailto:?body=")));
        const QString encoded = arg.mid(QStringLiteral("mailto:?body=").size());
        QVERIFY(!encoded.contains(QLatin1Char('&')));
        QVERIFY(!encoded.contains(QLatin1Char('=')));
        QVERIFY(!encoded.contains(QLatin1Char('?')));
        QVERIFY(!encoded.contains(QLatin1String("%0A"), Qt::CaseInsensitive));
        QCOMPARE(QUrl::fromPercentEncoding(encoded.toUtf8()), open);
    }

    // Safari: one target, opened through LaunchServices with the bundle
    // id -- no exec'ing the bundle binary, no profile/private arguments.
    void macOpenArgsSafari()
    {
        Target t;
        t.kind = Kind::BrowserProfile;
        t.engine = Engine::Generic;
        t.id = QStringLiteral("browser:macapp:com.apple.Safari:default");
        t.bundleId = QStringLiteral("com.apple.Safari");
        t.bundlePath = QStringLiteral("/Applications/Safari.app");
        t.exec = QStringLiteral("/Applications/Safari.app/Contents/MacOS/Safari");

        const auto args = macFixedOpenArgs(t, QStringLiteral("https://example.com"));
        QVERIFY(args.has_value());
        QCOMPARE(*args, QStringList({QStringLiteral("-b"), QStringLiteral("com.apple.Safari"),
                                     QStringLiteral("https://example.com")}));
    }

    // The fixed routes take argv from kind and bundle id alone. Hostile
    // exec and args fields are ignored: a Safari target whose exec names
    // a shell still resolves to `open -b`, and the email target ignores
    // its args list.
    void macOpenArgsIgnoreExecAndArgs()
    {
        Target safari;
        safari.kind = Kind::BrowserProfile;
        safari.id = QStringLiteral("browser:macapp:com.apple.Safari:default");
        safari.bundleId = QStringLiteral("com.apple.Safari");
        safari.exec = QStringLiteral("/bin/sh");
        safari.args = {QStringLiteral("-c"), QStringLiteral("echo pwned")};
        const auto sargs = macFixedOpenArgs(safari, QStringLiteral("https://example.com"));
        QVERIFY(sargs.has_value());
        QCOMPARE(*sargs, QStringList({QStringLiteral("-b"), QStringLiteral("com.apple.Safari"),
                                      QStringLiteral("https://example.com")}));

        Target mail;
        mail.kind = Kind::Action;
        mail.id = QStringLiteral("action:email");
        mail.exec = QStringLiteral("/bin/sh");
        mail.args = {QStringLiteral("rm"), QStringLiteral("-rf")};
        const auto margs = macFixedOpenArgs(mail, QStringLiteral("https://example.com"));
        QVERIFY(margs.has_value());
        QCOMPARE(margs->size(), 1);
        QVERIFY(margs->first().startsWith(QStringLiteral("mailto:?body=")));
    }

    // Non-Safari bundles keep the direct bundle-executable path; plain
    // Linux targets and other actions are untouched by the fixed route.
    void macOpenArgsDoNotEngageForOtherTargets()
    {
        Target brave;
        brave.kind = Kind::BrowserProfile;
        brave.engine = Engine::Chromium;
        brave.bundleId = QStringLiteral("com.brave.Browser");
        QVERIFY(!macFixedOpenArgs(brave, QStringLiteral("https://example.com")).has_value());

        Target linux;
        linux.kind = Kind::BrowserProfile;
        QVERIFY(!macFixedOpenArgs(linux, QStringLiteral("https://example.com")).has_value());

        Target copy;
        copy.kind = Kind::Action;
        copy.id = QStringLiteral("action:copy");
        QVERIFY(!macFixedOpenArgs(copy, QStringLiteral("https://example.com")).has_value());

        Target custom;
        custom.kind = Kind::Custom;
        QVERIFY(!macFixedOpenArgs(custom, QStringLiteral("https://example.com")).has_value());

        // An Action target with a different id does not take the mailto
        // route even on macOS.
        Target other;
        other.kind = Kind::Action;
        other.id = QStringLiteral("action:not-email");
        QVERIFY(!macFixedOpenArgs(other, QStringLiteral("https://example.com")).has_value());
    }

    // An owned route with an empty URL returns an engaged-but-empty list
    // so launchTarget() refuses rather than spawning a bare `open`
    // (which opens Finder on macOS).
    void macOpenArgsRefuseEmptyUrl()
    {
        Target mail;
        mail.kind = Kind::Action;
        mail.id = QStringLiteral("action:email");
        QCOMPARE(macFixedOpenArgs(mail, QString()).value_or(QStringList{QStringLiteral("x")}),
                 QStringList());

        Target safari;
        safari.kind = Kind::BrowserProfile;
        safari.bundleId = QStringLiteral("com.apple.Safari");
        QCOMPARE(macFixedOpenArgs(safari, QString()).value_or(QStringList{QStringLiteral("x")}),
                 QStringList());
    }

    // The builder is pure QString work: it cannot set XDG_ACTIVATION_TOKEN
    // or any other environment variable around the open spawn -- the
    // /usr/bin/open route in launchTarget() returns before the token
    // block that the exec path uses on Linux/Wayland.
    void macFixedRouteNeverTouchesActivationEnv()
    {
        qputenv("XDG_ACTIVATION_TOKEN", "sentinel");
        Target mail;
        mail.kind = Kind::Action;
        mail.id = QStringLiteral("action:email");
        QVERIFY(macFixedOpenArgs(mail, QStringLiteral("https://example.com")).has_value());
        QCOMPARE(qgetenv("XDG_ACTIVATION_TOKEN"), QByteArray("sentinel"));
        qunsetenv("XDG_ACTIVATION_TOKEN");
    }

    // Every exec name the macOS guard adds is refused by
    // isBlockedInterpreterChainMac() -- including `open` and `osascript` --
    // while the base chain leaves the mac-only names alone (osascript is
    // already in the base list). This pins the spec's refusal list even
    // on Linux where Q_OS_MACOS is off.
    void macChainBlocksMacOnlyNames()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QStringList macOnly = {QStringLiteral("open"), QStringLiteral("swift"),
                                     QStringLiteral("script"), QStringLiteral("tclsh"),
                                     QStringLiteral("wish"), QStringLiteral("shortcuts"),
                                     QStringLiteral("lldb"), QStringLiteral("automator")};
        for (const QString &name : macOnly) {
            const QString path = writePlainExec(dir, name);
            QVERIFY(!path.isEmpty());
            QVERIFY2(isBlockedInterpreterChainMac(path), qPrintable(name));
            QVERIFY2(!isBlockedInterpreterChain(path), qPrintable(name));
        }
        const QString osa = writePlainExec(dir, QStringLiteral("osascript"));
        QVERIFY(isBlockedInterpreterChainMac(osa));
        QVERIFY(isBlockedInterpreterChain(osa)); // already in the base list
    }

    // The macOS chain checks every symlink hop's basename: an innocently
    // named link to a file called `open` is refused on macOS and allowed
    // by the base chain.
    void macChainBlocksSymlinkToOpen()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString real = writePlainExec(dir, QStringLiteral("open"));
        QVERIFY(!real.isEmpty());
        const QString link = dir.filePath(QStringLiteral("totally-a-browser"));
        QVERIFY(QFile::link(real, link));

        QVERIFY(isBlockedInterpreterChainMac(link));
        QVERIFY(!isBlockedInterpreterChain(link));
    }

    // macOS refuses a custom command whose resolved path is a directory
    // or ends in .app. The base chain does not check either.
    void macChainBlocksDirectoryAndAppSuffix()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        QVERIFY(isBlockedInterpreterChainMac(dir.path()));
        QVERIFY(!isBlockedInterpreterChain(dir.path()));

        QVERIFY(QDir().mkpath(dir.filePath(QStringLiteral("Fake.app"))));
        const QString bundleDir = dir.filePath(QStringLiteral("Fake.app"));
        QVERIFY(isBlockedInterpreterChainMac(bundleDir));
        QVERIFY(!isBlockedInterpreterChain(bundleDir));

        // A non-directory path still ending in .app is refused too.
        const QString fileApp = writePlainExec(dir, QStringLiteral("Odd.app"));
        QVERIFY(!fileApp.isEmpty());
        QVERIFY(isBlockedInterpreterChainMac(fileApp));
        QVERIFY(!isBlockedInterpreterChain(fileApp));

        // But a real bundle executable (Contents/MacOS/<name>) is fine:
        // its leaf name carries no .app suffix.
        QVERIFY(QDir().mkpath(dir.filePath(QStringLiteral("Zen.app/Contents/MacOS"))));
        const QString zenExec = dir.filePath(QStringLiteral("Zen.app/Contents/MacOS/zen"));
        QFile z(zenExec);
        QVERIFY(z.open(QIODevice::WriteOnly));
        z.write("#!/bin/sh\n");
        z.close();
        QFile::setPermissions(zenExec, QFileDevice::ReadOwner | QFileDevice::WriteOwner
                                          | QFileDevice::ExeOwner);
        QVERIFY(!isBlockedInterpreterChainMac(zenExec));
        QVERIFY(!isBlockedInterpreterChain(zenExec));
    }

    // Linux behavior is unchanged end to end: an executable literally
    // named `open` launches like any other custom target (macOS refuses
    // it, which the macOS build covers via the osChainBlocked wiring).
    void linuxOpenCommandStillLaunches()
    {
#ifdef Q_OS_MACOS
        QSKIP("Linux parity check; on macOS `open` is refused");
#else
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString outPath = dir.filePath(QStringLiteral("argv.txt"));
        const QString open = writeDumpArgvScript(dir, QStringLiteral("open"), outPath);
        QVERIFY(!open.isEmpty());

        Target t;
        t.kind = Kind::Custom;
        t.exec = open;
        t.args = {QStringLiteral("$url")};
        QVERIFY(launchTarget(t, QStringLiteral("https://example.com")));
        QCOMPARE(readWhenReady(outPath), QByteArray("https://example.com\n"));
#endif
    }

    // Gecko/Chromium family targets exec the bundle's
    // Contents/MacOS/<executable> directly with the same argv the Linux
    // path builds: profile flag, private flag, URL last.
    void bundleExecutableArgvShape()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QVERIFY(QDir().mkpath(dir.filePath(QStringLiteral("Zen.app/Contents/MacOS"))));
        const QString outPath = dir.filePath(QStringLiteral("argv.txt"));
        const QString execPath = writeDumpArgvScript(
            dir, QStringLiteral("Zen.app/Contents/MacOS/zen"), outPath);
        QVERIFY(!execPath.isEmpty());

        Target t;
        t.kind = Kind::BrowserProfile;
        t.engine = Engine::Gecko;
        t.id = QStringLiteral("browser:macapp:app.zen.zen:private");
        t.bundleId = QStringLiteral("app.zen.zen");
        t.bundlePath = dir.filePath(QStringLiteral("Zen.app"));
        t.exec = execPath;
        t.args = {QStringLiteral("-P"), QStringLiteral("Work"),
                  QStringLiteral("--private-window"), QStringLiteral("$url")};
        QVERIFY(launchTarget(t, QStringLiteral("https://example.com/page")));
        QCOMPARE(readWhenReady(outPath),
                 QByteArray("-P\nWork\n--private-window\nhttps://example.com/page\n"));
    }

    // A non-web URL is refused before any route or exec work runs, for
    // mac-shaped targets as much as for ordinary ones.
    void nonWebUrlRefusedForMacTargets()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString execPath = writePlainExec(dir, QStringLiteral("browser-bin"));
        QVERIFY(!execPath.isEmpty());

        Target t;
        t.kind = Kind::BrowserProfile;
        t.engine = Engine::Chromium;
        t.bundleId = QStringLiteral("com.brave.Browser");
        t.exec = execPath;
        t.args = {QStringLiteral("$url")};
        QVERIFY(!launchTarget(t, QStringLiteral("file:///etc/passwd")));
        QVERIFY(!launchTarget(t, QStringLiteral("javascript:alert(1)")));
        QVERIFY(!launchTarget(t, QStringLiteral("mailto:a@b.c")));

        Target safari;
        safari.kind = Kind::BrowserProfile;
        safari.bundleId = QStringLiteral("com.apple.Safari");
        QVERIFY(!launchTarget(safari, QStringLiteral("file:///etc/passwd")));

        Target mail;
        mail.kind = Kind::Action;
        mail.id = QStringLiteral("action:email");
        QVERIFY(!launchTarget(mail, QStringLiteral("mailto:a@b.c")));
    }

#ifdef Q_OS_MACOS
    // The wiring that only exists on macOS: launchTarget() and
    // parseCustomCommand() route exec validation through
    // isBlockedInterpreterChainMac(). These launch refusals spawn
    // nothing. Positive `open -b`/`mailto:` spawns are not run here
    // because /usr/bin/open would reach real apps; argv is pinned by the
    // macFixedOpenArgs tests and the native gate covers real launches.
    void macLaunchRefusals()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString openPath = writePlainExec(dir, QStringLiteral("open"));
        QVERIFY(!openPath.isEmpty());

        Target t;
        t.kind = Kind::Custom;
        t.exec = openPath;
        t.args = {QStringLiteral("$url")};
        QVERIFY(!launchTarget(t, QStringLiteral("https://example.com")));

        t.exec = dir.path();
        QVERIFY(!launchTarget(t, QStringLiteral("https://example.com")));

        QVERIFY(QDir().mkpath(dir.filePath(QStringLiteral("Fake.app"))));
        t.exec = dir.filePath(QStringLiteral("Fake.app"));
        QVERIFY(!launchTarget(t, QStringLiteral("https://example.com")));

        Target parsed;
        QString error;
        QVERIFY(!parseCustomCommand(QStringLiteral("open https://example.com"), &parsed, &error));
        QCOMPARE(error, QStringLiteral("Shells and language runtimes are not allowed as handlers"));
    }
#endif
};

QTEST_MAIN(LaunchMacTest)
#include "test_launch_mac.moc"
