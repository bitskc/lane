#include "core/discovery.h"

#include <QDir>
#include <QFileInfo>
#include <QTemporaryDir>
#include <algorithm>
#include <QFile>
#include <QStandardPaths>
#include <QTest>

using namespace Lane;

static QString fixtureRoot()
{
    return QStringLiteral(LANE_FIXTURE_ROOT);
}

// Recursively mirrors `src` into `dst`, overwriting any existing files.
// Used to stage a whole Gecko profile tree (profiles.ini plus each
// profile's directory, so directory-existence and prefs.js checks in
// geckoProfiles() see real files) into a synthesized configHome.
static void copyTree(const QString &src, const QString &dst)
{
    const QDir srcDir(src);
    QDir().mkpath(dst);
    const auto entries = srcDir.entryList(QDir::NoDotAndDotDot | QDir::AllEntries);
    for (const auto &entry : entries) {
        const QString srcPath = src + QLatin1Char('/') + entry;
        const QString dstPath = dst + QLatin1Char('/') + entry;
        if (QFileInfo(srcPath).isDir()) {
            copyTree(srcPath, dstPath);
        } else {
            QFile::remove(dstPath);
            QFile::copy(srcPath, dstPath);
        }
    }
}

class DiscoveryTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void discoversProfilesAndPwas()
    {
        DiscoveryPaths p;
        p.home = fixtureRoot();
        p.configHome = fixtureRoot() + QStringLiteral("/config");
        p.dataHome = fixtureRoot() + QStringLiteral("/pwa");
        p.applicationDirs = {fixtureRoot() + QStringLiteral("/desktop")};

        // Stage full Gecko profile trees (profiles.ini plus each profile's
        // directory and prefs.js) into a synthesized configHome, the way a
        // real ~/.config/zen or ~/.config/mozilla/firefox looks.
        QDir().mkpath(p.configHome + QStringLiteral("/BraveSoftware/Brave-Browser/Default"));
        copyTree(fixtureRoot() + QStringLiteral("/gecko/zen"), p.configHome + QStringLiteral("/zen"));
        copyTree(fixtureRoot() + QStringLiteral("/gecko/firefox"), p.configHome + QStringLiteral("/mozilla/firefox"));
        QFile::remove(p.configHome + QStringLiteral("/BraveSoftware/Brave-Browser/Local State"));
        QFile::copy(fixtureRoot() + QStringLiteral("/chromium/BraveSoftware/Brave-Browser/Local State"),
                    p.configHome + QStringLiteral("/BraveSoftware/Brave-Browser/Local State"));

        const auto targets = discoverTargets(p);
        QStringList ids;
        QStringList names;
        for (const auto &t : targets) {
            ids << t.id;
            names << t.displayName();
        }

        QVERIFY(ids.contains(QStringLiteral("pwa:01KV62HYP6YRMPPPBMVTRA23HX")));
        QVERIFY(ids.contains(QStringLiteral("action:copy")));
        QVERIFY(!ids.filter(QStringLiteral("app.lane")).size());

        const bool hasZen = std::any_of(targets.begin(), targets.end(), [](const Target &t) {
            return t.browserName == QLatin1String("Zen") && !t.incognito;
        });
        QVERIFY(hasZen);

        const bool hasBravePersonal = std::any_of(targets.begin(), targets.end(), [](const Target &t) {
            return t.browserName == QLatin1String("Brave") && t.name == QLatin1String("Personal") && !t.incognito;
        });
        QVERIFY(hasBravePersonal);

        const bool skippedBackup = std::any_of(targets.begin(), targets.end(), [](const Target &t) {
            return t.profileDir.contains(QLatin1String("backup"));
        });
        QVERIFY(!skippedBackup);

        const bool qboDesktopName = std::any_of(targets.begin(), targets.end(), [](const Target &t) {
            return t.id == QLatin1String("pwa:01KT1N5RHR7DYJ54DWPBWAY5V4") && t.name == QLatin1String("QBO");
        });
        QVERIFY(qboDesktopName);

        // Zen has three real, initialized profiles in the fixture (Default
        // Profile, Default (release), and Work); "missing-profile" is listed
        // in profiles.ini but has no directory on disk and must be skipped.
        const qsizetype zenProfileCount = std::count_if(targets.begin(), targets.end(), [](const Target &t) {
            return t.browserName == QLatin1String("Zen") && t.kind == Kind::BrowserProfile && !t.incognito;
        });
        QCOMPARE(zenProfileCount, 3);
        QVERIFY(!std::any_of(targets.begin(), targets.end(), [](const Target &t) {
            return t.profileDir.contains(QLatin1String("missing-profile"));
        }));

        // Two desktop files (zen.desktop, zen-browser.desktop) resolve to the
        // same Zen install and profile store; they must not double the list,
        // and the deterministic tie-break must not pick the also-installed
        // duplicate's icon.
        QVERIFY(!std::any_of(targets.begin(), targets.end(), [](const Target &t) {
            return t.icon == QLatin1String("zen-browser-duplicate");
        }));

        // Zen's Install-default profile (an internal name like
        // "Default (release)") is labelled plain "Default", not the raw
        // profiles.ini name.
        const auto zenDefault = std::find_if(targets.begin(), targets.end(), [](const Target &t) {
            return t.browserName == QLatin1String("Zen") && t.isBrowserDefault && !t.incognito;
        });
        QVERIFY(zenDefault != targets.end());
        QCOMPARE(zenDefault->displayName(), QStringLiteral("Zen · Default"));

        // Launch args identify the profile by its absolute directory via
        // --profile, not the internal "-P <name>" form (Zen's internal names
        // have spaces and parentheses that don't round-trip through -P).
        QCOMPARE(zenDefault->args.size(), 4);
        QCOMPARE(zenDefault->args.at(0), QStringLiteral("--profile"));
        QCOMPARE(zenDefault->args.at(1), zenDefault->profileDir);
        QVERIFY(QDir::isAbsolutePath(zenDefault->args.at(1)));
        QCOMPARE(zenDefault->args.at(2), QStringLiteral("--new-tab"));

        // A profile whose folder-name suffix ("Work") is more human than its
        // generic profiles.ini Name ("default-release") is labelled with the
        // suffix instead.
        const bool hasWorkProfile = std::any_of(targets.begin(), targets.end(), [](const Target &t) {
            return t.browserName == QLatin1String("Zen") && t.name == QLatin1String("Work") && !t.incognito;
        });
        QVERIFY(hasWorkProfile);

        // Classic Firefox (no Install section) still honours the legacy
        // per-profile Default=1 marker and gives it the same "Default" label.
        const bool hasFirefoxDefault = std::any_of(targets.begin(), targets.end(), [](const Target &t) {
            return t.browserName == QLatin1String("Firefox") && t.name == QLatin1String("Default") && !t.incognito;
        });
        QVERIFY(hasFirefoxDefault);
    }

    void discoversGeckoContainers()
    {
        DiscoveryPaths p;
        p.home = fixtureRoot();
        p.configHome = fixtureRoot() + QStringLiteral("/config");
        p.dataHome = fixtureRoot() + QStringLiteral("/pwa");
        p.applicationDirs = {fixtureRoot() + QStringLiteral("/desktop")};

        copyTree(fixtureRoot() + QStringLiteral("/gecko/zen"), p.configHome + QStringLiteral("/zen"));
        copyTree(fixtureRoot() + QStringLiteral("/gecko/firefox"), p.configHome + QStringLiteral("/mozilla/firefox"));

        const auto targets = discoverTargets(p);

        // The install-default Zen profile ships a protocol-handler extension
        // (Open URL in Container) in its extensions.json fixture, so its
        // public containers become targets.
        const auto work = std::find_if(targets.begin(), targets.end(), [](const Target &t) {
            return t.kind == Kind::Container && t.browserName.contains(QLatin1String("Zen"))
                && t.containerName == QLatin1String("Work");
        });
        QVERIFY(work != targets.end());
        QVERIFY(work->id.startsWith(QLatin1String("browser:")));
        QVERIFY(work->id.contains(QLatin1String(":container:2")));
        QVERIFY(work->displayName().contains(QLatin1String("Work")));
        QVERIFY(work->args.contains(QStringLiteral("--new-tab")));
        const bool hasUrlEncodedArg = std::any_of(work->args.begin(), work->args.end(), [](const QString &a) {
            return a.contains(QLatin1String("$urlEncoded"));
        });
        QVERIFY(hasUrlEncodedArg);
        const bool hasContainerArg = std::any_of(work->args.begin(), work->args.end(), [](const QString &a) {
            return a.contains(QLatin1String("ext+container:name=Work"));
        });
        QVERIFY(hasContainerArg);
        QCOMPARE(work->containerId, 2);
        QVERIFY(!work->incognito);

        const bool hasDev = std::any_of(targets.begin(), targets.end(), [](const Target &t) {
            return t.kind == Kind::Container && t.containerName == QLatin1String("Dev");
        });
        QVERIFY(hasDev);

        // The internal placeholder identity (public: false) never becomes a target.
        const bool hasInternal = std::any_of(targets.begin(), targets.end(), [](const Target &t) {
            return t.kind == Kind::Container && t.name.startsWith(QLatin1String("userContextIdInternal"));
        });
        QVERIFY(!hasInternal);

        // No container target is ever incognito; containers never mix with
        // private-window rows.
        const bool anyIncognitoContainer = std::any_of(targets.begin(), targets.end(), [](const Target &t) {
            return t.kind == Kind::Container && t.incognito;
        });
        QVERIFY(!anyIncognitoContainer);

        // The second Zen profile (qq35x6ld.Default Profile) only has the
        // four stock l10n containers and no protocol-handler extension: it
        // must not grow any container targets.
        const bool secondProfileHasContainers = std::any_of(targets.begin(), targets.end(), [](const Target &t) {
            return t.kind == Kind::Container && t.profileDir.contains(QLatin1String("qq35x6ld"));
        });
        QVERIFY(!secondProfileHasContainers);

        // Firefox profiles in the fixture have no containers.json at all.
        const bool firefoxHasContainers = std::any_of(targets.begin(), targets.end(), [](const Target &t) {
            return t.kind == Kind::Container && t.browserName.contains(QLatin1String("Firefox"));
        });
        QVERIFY(!firefoxHasContainers);
    }

    void discoversFlatpakZenProfile()
    {
        // Regression for: a Zen install packaged as a Flatpak has a
        // desktop entry Lane can find, but its real profile store lives
        // under ~/.var/app/<app-id>/.zen rather than ~/.config/zen or
        // ~/.zen, and its Exec= line runs "flatpak run ... <app-id>"
        // rather than the browser binary directly.
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString root = tmp.path();

        QDir().mkpath(root + QStringLiteral("/apps"));
        QFile desktop(root + QStringLiteral("/apps/app.zen_browser.zen.desktop"));
        QVERIFY(desktop.open(QIODevice::WriteOnly));
        desktop.write(QByteArrayLiteral(
            "[Desktop Entry]\n"
            "Name=Zen Browser\n"
            "Exec=/usr/bin/flatpak run --branch=stable --arch=x86_64 --command=zen app.zen_browser.zen %u\n"
            "Icon=app.zen_browser.zen\n"
            "Type=Application\n"
            "Categories=Network;WebBrowser;\n"
            "MimeType=x-scheme-handler/http;x-scheme-handler/https;\n"));
        desktop.close();

        copyTree(fixtureRoot() + QStringLiteral("/gecko/zen"),
                  root + QStringLiteral("/home/.var/app/app.zen_browser.zen/.zen"));

        DiscoveryPaths p;
        p.home = root + QStringLiteral("/home");
        p.configHome = root + QStringLiteral("/config");
        p.dataHome = root + QStringLiteral("/data");
        p.applicationDirs = {root + QStringLiteral("/apps")};

        const auto targets = discoverTargets(p);

        // The real profiles under .var/app were found (three, same as the
        // native-install fixture), not the synthetic ":default" fallback
        // discovery emits when profiles.ini can't be located at all.
        const qsizetype zenProfileCount = std::count_if(targets.begin(), targets.end(), [](const Target &t) {
            return t.browserName == QLatin1String("Zen") && t.kind == Kind::BrowserProfile && !t.incognito;
        });
        QCOMPARE(zenProfileCount, 3);
        QVERIFY(!std::any_of(targets.begin(), targets.end(), [](const Target &t) {
            return t.id == QLatin1String("browser:app.zen_browser.zen:default");
        }));

        const auto zenDefault = std::find_if(targets.begin(), targets.end(), [](const Target &t) {
            return t.browserName == QLatin1String("Zen") && t.isBrowserDefault && !t.incognito;
        });
        QVERIFY(zenDefault != targets.end());
        QVERIFY(zenDefault->profileDir.contains(QLatin1String("/.var/app/app.zen_browser.zen/.zen")));

        // The argv is the Flatpak wrapper's own tokens followed by Lane's
        // profile args, not "flatpak --profile ..." (which would silently
        // fail: flatpak has no --profile option of its own). The --profile
        // dir is the in-sandbox spelling: inside the sandbox the profile
        // store is mounted at ~/.zen, not at the host's
        // ~/.var/app/<app-id>/.zen path, and the running instance's
        // remoting name is derived from that spelling. Passing the host
        // path makes Zen report "already running but not responding".
        const QString sandboxProfileDir =
            QString(zenDefault->profileDir).replace(root + QStringLiteral("/home/.var/app/app.zen_browser.zen/"),
                                                    root + QStringLiteral("/home/"));
        QVERIFY(sandboxProfileDir != zenDefault->profileDir);
        QCOMPARE(zenDefault->exec, QStringLiteral("/usr/bin/flatpak"));
        QCOMPARE(zenDefault->args,
                 QStringList({QStringLiteral("run"), QStringLiteral("--branch=stable"), QStringLiteral("--arch=x86_64"),
                              QStringLiteral("--command=zen"), QStringLiteral("app.zen_browser.zen"), QStringLiteral("--profile"),
                              sandboxProfileDir, QStringLiteral("--new-tab"), QStringLiteral("$url")}));

        // Containers still come through for the Flatpak profile store,
        // carrying the same Flatpak argv prefix.
        const auto work = std::find_if(targets.begin(), targets.end(), [](const Target &t) {
            return t.kind == Kind::Container && t.browserName.contains(QLatin1String("Zen")) && t.containerName == QLatin1String("Work");
        });
        QVERIFY(work != targets.end());
        QCOMPARE(work->exec, QStringLiteral("/usr/bin/flatpak"));
        QVERIFY(work->args.contains(QStringLiteral("app.zen_browser.zen")));
        QVERIFY(work->args.contains(QStringLiteral("--profile")));
        QVERIFY(!work->args.contains(zenDefault->profileDir));
    }

    void stripsFlatpakFileForwardingMarkersFromExecPrefix()
    {
        // Regression for: flatpak's exporter rewrites Exec= for apps that
        // register as URL handlers to wrap the %u field code in a
        // file-forwarding span, "@@u %u @@" (this is the real Exec= line
        // flathub's app.zen_browser.zen desktop entry ships). The opening
        // marker is "@@u", not bare "@@"; treating only exact "@@" as a
        // field code left "@@u" in Lane's argv prefix, where it corrupts
        // flatpak run's own argv parsing (flatpak itself reads "@@u ... @@"
        // as a forwarding span). "--file-forwarding" and the app id are
        // real flatpak run arguments and must survive, only the "@@u",
        // "%u", and "@@" markers are stripped.
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString root = tmp.path();

        QDir().mkpath(root + QStringLiteral("/apps"));
        QFile desktop(root + QStringLiteral("/apps/app.zen_browser.zen.desktop"));
        QVERIFY(desktop.open(QIODevice::WriteOnly));
        desktop.write(QByteArrayLiteral(
            "[Desktop Entry]\n"
            "Name=Zen Browser\n"
            "Exec=/usr/bin/flatpak run --branch=stable --arch=x86_64 --command=launch-script.sh "
            "--file-forwarding app.zen_browser.zen @@u %u @@\n"
            "Icon=app.zen_browser.zen\n"
            "Type=Application\n"
            "Categories=Network;WebBrowser;\n"
            "MimeType=x-scheme-handler/http;x-scheme-handler/https;\n"));
        desktop.close();

        copyTree(fixtureRoot() + QStringLiteral("/gecko/zen"),
                  root + QStringLiteral("/home/.var/app/app.zen_browser.zen/.zen"));

        DiscoveryPaths p;
        p.home = root + QStringLiteral("/home");
        p.configHome = root + QStringLiteral("/config");
        p.dataHome = root + QStringLiteral("/data");
        p.applicationDirs = {root + QStringLiteral("/apps")};

        const auto targets = discoverTargets(p);
        const auto zenDefault = std::find_if(targets.begin(), targets.end(), [](const Target &t) {
            return t.browserName == QLatin1String("Zen") && t.isBrowserDefault && !t.incognito;
        });
        QVERIFY(zenDefault != targets.end());

        QVERIFY(!std::any_of(zenDefault->args.begin(), zenDefault->args.end(), [](const QString &a) {
            return a.startsWith(QLatin1String("@@"));
        }));
        QCOMPARE(zenDefault->exec, QStringLiteral("/usr/bin/flatpak"));
        QCOMPARE(zenDefault->args,
                 QStringList({QStringLiteral("run"), QStringLiteral("--branch=stable"), QStringLiteral("--arch=x86_64"),
                              QStringLiteral("--command=launch-script.sh"), QStringLiteral("--file-forwarding"),
                              QStringLiteral("app.zen_browser.zen"), QStringLiteral("--profile"),
                              QString(zenDefault->profileDir).replace(root + QStringLiteral("/home/.var/app/app.zen_browser.zen/"),
                                                                      root + QStringLiteral("/home/")),
                              QStringLiteral("--new-tab"), QStringLiteral("$url")}));
    }

    void discoversFlatpakChromiumProfile()
    {
        // Same bug class as discoversFlatpakZenProfile() but for a
        // Chromium-based browser: its profile store lives under
        // ~/.var/app/<app-id>/config/... instead of ~/.config/....
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString root = tmp.path();

        QDir().mkpath(root + QStringLiteral("/apps"));
        QFile desktop(root + QStringLiteral("/apps/com.brave.Browser.desktop"));
        QVERIFY(desktop.open(QIODevice::WriteOnly));
        desktop.write(QByteArrayLiteral(
            "[Desktop Entry]\n"
            "Name=Brave Browser\n"
            "Exec=/usr/bin/flatpak run --branch=stable --arch=x86_64 --command=brave com.brave.Browser %u\n"
            "Icon=com.brave.Browser\n"
            "Type=Application\n"
            "Categories=Network;WebBrowser;\n"
            "MimeType=x-scheme-handler/http;x-scheme-handler/https;\n"));
        desktop.close();

        const QString flatpakBraveDir =
            root + QStringLiteral("/home/.var/app/com.brave.Browser/config/BraveSoftware/Brave-Browser");
        QDir().mkpath(flatpakBraveDir);
        QFile::copy(fixtureRoot() + QStringLiteral("/chromium/BraveSoftware/Brave-Browser/Local State"),
                    flatpakBraveDir + QStringLiteral("/Local State"));

        DiscoveryPaths p;
        p.home = root + QStringLiteral("/home");
        p.configHome = root + QStringLiteral("/config");
        p.dataHome = root + QStringLiteral("/data");
        p.applicationDirs = {root + QStringLiteral("/apps")};

        const auto targets = discoverTargets(p);

        const auto personal = std::find_if(targets.begin(), targets.end(), [](const Target &t) {
            return t.browserName == QLatin1String("Brave") && t.name == QLatin1String("Personal") && !t.incognito;
        });
        QVERIFY(personal != targets.end());
        QVERIFY(personal->profileDir.contains(QLatin1String("/.var/app/com.brave.Browser/config/BraveSoftware/Brave-Browser")));
        QCOMPARE(personal->exec, QStringLiteral("/usr/bin/flatpak"));
        QCOMPARE(personal->args,
                 QStringList({QStringLiteral("run"), QStringLiteral("--branch=stable"), QStringLiteral("--arch=x86_64"),
                              QStringLiteral("--command=brave"), QStringLiteral("com.brave.Browser"),
                              QStringLiteral("--profile-directory=Default"), QStringLiteral("--new-tab"), QStringLiteral("$url")}));
    }

    void fingerprintPrefersRicherGeckoDataDir()
    {
        // Regression for: when more than one Gecko data-dir candidate has a
        // profiles.ini (e.g. a stale ~/.config/zen left over next to a real
        // ~/.zen), the candidate whose Profile* entries actually resolve to
        // directories on disk must win, not just whichever is checked first.
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString root = tmp.path();

        QDir().mkpath(root + QStringLiteral("/apps"));
        QFile desktop(root + QStringLiteral("/apps/zen.desktop"));
        QVERIFY(desktop.open(QIODevice::WriteOnly));
        desktop.write(QByteArrayLiteral(
            "[Desktop Entry]\n"
            "Name=Zen Browser\n"
            "Exec=/opt/zen-browser-bin/zen-bin %u\n"
            "Icon=zen-browser\n"
            "Type=Application\n"
            "Categories=Network;WebBrowser;\n"
            "MimeType=x-scheme-handler/http;x-scheme-handler/https;\n"));
        desktop.close();

        // configHome/zen: profiles.ini exists but its one entry points at a
        // directory that was never created (score 0).
        QDir().mkpath(root + QStringLiteral("/config/zen"));
        QFile sparse(root + QStringLiteral("/config/zen/profiles.ini"));
        QVERIFY(sparse.open(QIODevice::WriteOnly));
        sparse.write(QByteArrayLiteral(
            "[Profile0]\nName=Sparse\nIsRelative=1\nPath=sparse-profile\n"));
        sparse.close();

        // home/.zen: profiles.ini with two entries whose directories really
        // exist (score 2). This is the one actually in use.
        QDir().mkpath(root + QStringLiteral("/home/.zen/rich1"));
        QDir().mkpath(root + QStringLiteral("/home/.zen/rich2"));
        QVERIFY(QFile(root + QStringLiteral("/home/.zen/rich1/prefs.js")).open(QIODevice::WriteOnly));
        QVERIFY(QFile(root + QStringLiteral("/home/.zen/rich2/prefs.js")).open(QIODevice::WriteOnly));
        QFile rich(root + QStringLiteral("/home/.zen/profiles.ini"));
        QVERIFY(rich.open(QIODevice::WriteOnly));
        rich.write(QByteArrayLiteral(
            "[Profile0]\nName=Rich1\nIsRelative=1\nPath=rich1\n\n"
            "[Profile1]\nName=Rich2\nIsRelative=1\nPath=rich2\n"));
        rich.close();

        DiscoveryPaths p;
        p.home = root + QStringLiteral("/home");
        p.configHome = root + QStringLiteral("/config");
        p.dataHome = root + QStringLiteral("/data");
        p.applicationDirs = {root + QStringLiteral("/apps")};

        const auto targets = discoverTargets(p);
        const qsizetype zenProfileCount = std::count_if(targets.begin(), targets.end(), [](const Target &t) {
            return t.browserName == QLatin1String("Zen") && t.kind == Kind::BrowserProfile && !t.incognito;
        });
        QCOMPARE(zenProfileCount, 2);
        QVERIFY(!std::any_of(targets.begin(), targets.end(), [](const Target &t) {
            return t.name == QLatin1String("Sparse");
        }));
    }

    void applyConfigAliasesAndOrder()
    {
        QList<Target> targets;
        Target a;
        a.id = QStringLiteral("browser:zen:def");
        a.kind = Kind::BrowserProfile;
        a.name = QStringLiteral("Default");
        a.browserName = QStringLiteral("Zen");
        Target b;
        b.id = QStringLiteral("browser:brave:personal");
        b.kind = Kind::BrowserProfile;
        b.name = QStringLiteral("Personal");
        b.browserName = QStringLiteral("Brave");
        Target c;
        c.id = QStringLiteral("pwa:gh");
        c.kind = Kind::Pwa;
        c.name = QStringLiteral("GitHub");
        targets = {a, b, c};

        Config cfg;
        cfg.targetOrder = {QStringLiteral("pwa:gh"), QStringLiteral("browser:brave:personal")};
        cfg.targetAliases.insert(QStringLiteral("browser:zen:def"), QStringLiteral("My Browser"));

        const auto result = applyConfigToTargets(targets, cfg);

        // Alias applied
        const bool hasAlias = std::any_of(result.begin(), result.end(), [](const Target &t) {
            return t.id == QLatin1String("browser:zen:def") && t.customName == QLatin1String("My Browser");
        });
        QVERIFY(hasAlias);

        // Order: listed ids first in listed order, then remaining in original order
        QCOMPARE(result.at(0).id, QStringLiteral("pwa:gh"));
        QCOMPARE(result.at(1).id, QStringLiteral("browser:brave:personal"));
        QCOMPARE(result.at(2).id, QStringLiteral("browser:zen:def"));

        // displayName uses alias
        const bool displayNameUsesAlias = std::any_of(result.begin(), result.end(), [](const Target &t) {
            return t.id == QLatin1String("browser:zen:def") && t.displayName() == QLatin1String("My Browser");
        });
        QVERIFY(displayNameUsesAlias);
    }

    void applyConfigHiddenFlagIsIdempotentAcrossReapplication()
    {
        // Controller::hideTarget()/renameTarget()/moveTarget() reapply
        // config onto the already-applied m_targets list instead of a
        // freshly discovered one (to skip re-scanning everything on a
        // purely cosmetic change), so applyConfigToTargets() must be safe
        // to call more than once on the same list: hiding a target and
        // then un-hiding it must actually clear the flag again, not leave
        // it stuck true from the first call.
        QList<Target> targets;
        Target a;
        a.id = QStringLiteral("browser:zen:def");
        a.kind = Kind::BrowserProfile;
        a.name = QStringLiteral("Default");
        a.browserName = QStringLiteral("Zen");
        targets = {a};

        Config cfg;
        cfg.hiddenTargetIds = {QStringLiteral("browser:zen:def")};
        auto result = applyConfigToTargets(targets, cfg);
        QVERIFY(result.at(0).hidden);

        // Un-hide, then reapply onto the *result* of the first call, the
        // way Controller does.
        cfg.hiddenTargetIds.clear();
        result = applyConfigToTargets(result, cfg);
        QVERIFY(!result.at(0).hidden);
    }

    void applyConfigCustomTargetsAppendAndRemoveOnReapplication()
    {
        // Controller::addCustomTarget()/removeCustomTarget() reapply config
        // onto the already-applied m_targets list instead of a freshly
        // discovered one (to skip re-scanning everything when a custom
        // target is added or removed), so applyConfigToTargets() must
        // append a newly-added custom target when reapplied onto an
        // already-applied list, and must not resurrect a custom target
        // that was removed from config after Controller erased it from the
        // list by id.
        QList<Target> targets;
        Target a;
        a.id = QStringLiteral("browser:zen:def");
        a.kind = Kind::BrowserProfile;
        a.name = QStringLiteral("Default");
        a.browserName = QStringLiteral("Zen");
        targets = {a};

        Config cfg;
        Target c1;
        c1.id = QStringLiteral("custom:first");
        c1.kind = Kind::Custom;
        c1.name = QStringLiteral("First");
        c1.exec = QStringLiteral("/usr/bin/first");
        cfg.customTargets = {c1};

        auto result = applyConfigToTargets(targets, cfg);
        QCOMPARE(result.size(), 2);
        QCOMPARE(result.at(1).id, QStringLiteral("custom:first"));

        // addCustomTarget: append a fresh custom target to config, reapply
        // onto the *result* of the previous call, the way Controller does.
        Target c2;
        c2.id = QStringLiteral("custom:second");
        c2.kind = Kind::Custom;
        c2.name = QStringLiteral("Second");
        c2.exec = QStringLiteral("/usr/bin/second");
        cfg.customTargets.append(c2);
        result = applyConfigToTargets(result, cfg);
        QCOMPARE(result.size(), 3);
        QCOMPARE(result.at(2).id, QStringLiteral("custom:second"));

        // removeCustomTarget: drop the entry from config and erase it from
        // the list by id, then reapply; the removed target must not come
        // back and the remaining entries must be untouched.
        cfg.customTargets = {c2};
        QList<Target> remaining;
        for (const auto &t : result) {
            if (t.id != QLatin1String("custom:first")) {
                remaining.append(t);
            }
        }
        result = applyConfigToTargets(remaining, cfg);
        QCOMPARE(result.size(), 2);
        QCOMPARE(result.at(0).id, QStringLiteral("browser:zen:def"));
        QCOMPARE(result.at(1).id, QStringLiteral("custom:second"));
    }

    void applyConfigEmptyOrderKeepsDiscoveryOrder()
    {
        QList<Target> targets;
        Target a;
        a.id = QStringLiteral("browser:zen:def");
        a.kind = Kind::BrowserProfile;
        a.name = QStringLiteral("Default");
        a.browserName = QStringLiteral("Zen");
        Target b;
        b.id = QStringLiteral("browser:brave:personal");
        b.kind = Kind::BrowserProfile;
        b.name = QStringLiteral("Personal");
        b.browserName = QStringLiteral("Brave");
        targets = {a, b};

        Config cfg;
        const auto result = applyConfigToTargets(targets, cfg);
        QCOMPARE(result.at(0).id, QStringLiteral("browser:zen:def"));
        QCOMPARE(result.at(1).id, QStringLiteral("browser:brave:personal"));
    }

    void applyConfigOrderSkipsMissingIds()
    {
        QList<Target> targets;
        Target a;
        a.id = QStringLiteral("browser:zen:def");
        a.kind = Kind::BrowserProfile;
        a.name = QStringLiteral("Default");
        a.browserName = QStringLiteral("Zen");
        Target b;
        b.id = QStringLiteral("browser:brave:personal");
        b.kind = Kind::BrowserProfile;
        b.name = QStringLiteral("Personal");
        b.browserName = QStringLiteral("Brave");
        targets = {a, b};

        Config cfg;
        cfg.targetOrder = {QStringLiteral("browser:brave:personal"), QStringLiteral("nonexistent:id")};
        const auto result = applyConfigToTargets(targets, cfg);
        QCOMPARE(result.at(0).id, QStringLiteral("browser:brave:personal"));
        QCOMPARE(result.at(1).id, QStringLiteral("browser:zen:def"));
    }

    void moveIdAmongSiblingsSkipsIncognito()
    {
        QList<Target> targets;
        Target zen;
        zen.id = QStringLiteral("zen");
        zen.kind = Kind::BrowserProfile;
        zen.incognito = false;
        Target zenPrivate;
        zenPrivate.id = QStringLiteral("zen-private");
        zenPrivate.kind = Kind::BrowserProfile;
        zenPrivate.incognito = true;
        Target brave;
        brave.id = QStringLiteral("brave");
        brave.kind = Kind::BrowserProfile;
        brave.incognito = false;
        targets = {zen, zenPrivate, brave};

        const auto result = moveIdAmongSiblings(targets, QStringLiteral("brave"), 0);
        QCOMPARE(result, QStringList({QStringLiteral("brave"), QStringLiteral("zen-private"), QStringLiteral("zen")}));
    }

    void moveIdAmongSiblingsUnknownId()
    {
        QList<Target> targets;
        Target zen;
        zen.id = QStringLiteral("zen");
        zen.kind = Kind::BrowserProfile;
        Target brave;
        brave.id = QStringLiteral("brave");
        brave.kind = Kind::BrowserProfile;
        targets = {zen, brave};

        const auto result = moveIdAmongSiblings(targets, QStringLiteral("nonexistent"), 0);
        QCOMPARE(result, QStringList({QStringLiteral("zen"), QStringLiteral("brave")}));
    }

    void moveIdAmongSiblingsClampsIndex()
    {
        QList<Target> targets;
        Target zen;
        zen.id = QStringLiteral("zen");
        zen.kind = Kind::BrowserProfile;
        Target brave;
        brave.id = QStringLiteral("brave");
        brave.kind = Kind::BrowserProfile;
        Target firefox;
        firefox.id = QStringLiteral("firefox");
        firefox.kind = Kind::BrowserProfile;
        targets = {zen, brave, firefox};

        const auto result = moveIdAmongSiblings(targets, QStringLiteral("zen"), 99);
        QCOMPARE(result, QStringList({QStringLiteral("brave"), QStringLiteral("firefox"), QStringLiteral("zen")}));
    }

    void nativeExecFlagsDoNotLeakIntoArgv()
    {
        // Regression for issue #12: a native Exec= line's own flags must
        // not be prepended to Lane's argv. For
        // "Exec=/usr/bin/firefox --new-window %u" the rebuilt argv used to
        // start with "--new-window", which then ate "--profile" as the
        // URL to open. Only Flatpak entries keep their Exec= prefix.
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString root = tmp.path();

        QDir().mkpath(root + QStringLiteral("/apps"));
        QFile desktop(root + QStringLiteral("/apps/firefox.desktop"));
        QVERIFY(desktop.open(QIODevice::WriteOnly));
        desktop.write(QByteArrayLiteral(
            "[Desktop Entry]\n"
            "Name=Firefox\n"
            "Exec=/usr/bin/firefox --new-window %u\n"
            "Icon=firefox\n"
            "Type=Application\n"
            "Categories=Network;WebBrowser;\n"
            "MimeType=x-scheme-handler/http;x-scheme-handler/https;\n"));
        desktop.close();

        copyTree(fixtureRoot() + QStringLiteral("/gecko/firefox"),
                  root + QStringLiteral("/config/mozilla/firefox"));

        DiscoveryPaths p;
        p.home = root + QStringLiteral("/home");
        p.configHome = root + QStringLiteral("/config");
        p.dataHome = root + QStringLiteral("/data");
        p.applicationDirs = {root + QStringLiteral("/apps")};

        const auto targets = discoverTargets(p);
        const auto def = std::find_if(targets.begin(), targets.end(), [](const Target &t) {
            return t.browserName == QLatin1String("Firefox") && t.kind == Kind::BrowserProfile && !t.incognito;
        });
        QVERIFY(def != targets.end());
        QCOMPARE(def->exec, QStringLiteral("/usr/bin/firefox"));
        QVERIFY(!def->args.contains(QStringLiteral("--new-window")));
        QVERIFY(def->args.contains(QStringLiteral("--new-tab")));
    }

    void envWrappedExecUnwrapsToRealProgram()
    {
        // "Exec=env MOZ_X11=1 firefox %u" must discover as firefox, not as
        // exec=env (which the launcher's interpreter blocklist rejects, so
        // the target would appear in settings but fail on every launch).
        // An Exec= that unwraps to no program at all is dropped entirely.
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString root = tmp.path();

        QDir().mkpath(root + QStringLiteral("/apps"));
        QFile desktop(root + QStringLiteral("/apps/firefox.desktop"));
        QVERIFY(desktop.open(QIODevice::WriteOnly));
        desktop.write(QByteArrayLiteral(
            "[Desktop Entry]\n"
            "Name=Firefox\n"
            "Exec=env MOZ_X11=1 firefox %u\n"
            "Icon=firefox\n"
            "Type=Application\n"
            "Categories=Network;WebBrowser;\n"
            "MimeType=x-scheme-handler/http;x-scheme-handler/https;\n"));
        desktop.close();

        QFile dead(root + QStringLiteral("/apps/dead.desktop"));
        QVERIFY(dead.open(QIODevice::WriteOnly));
        dead.write(QByteArrayLiteral(
            "[Desktop Entry]\n"
            "Name=Dead\n"
            "Exec=env MOZ_X11=1\n"
            "Type=Application\n"
            "Categories=Network;WebBrowser;\n"
            "MimeType=x-scheme-handler/http;\n"));
        dead.close();

        copyTree(fixtureRoot() + QStringLiteral("/gecko/firefox"),
                  root + QStringLiteral("/config/mozilla/firefox"));

        DiscoveryPaths p;
        p.home = root + QStringLiteral("/home");
        p.configHome = root + QStringLiteral("/config");
        p.dataHome = root + QStringLiteral("/data");
        p.applicationDirs = {root + QStringLiteral("/apps")};

        const auto targets = discoverTargets(p);
        const auto def = std::find_if(targets.begin(), targets.end(), [](const Target &t) {
            return t.browserName == QLatin1String("Firefox") && t.kind == Kind::BrowserProfile && !t.incognito;
        });
        QVERIFY(def != targets.end());
        QCOMPARE(def->exec, QStringLiteral("firefox"));
        QVERIFY(!def->args.contains(QStringLiteral("MOZ_X11=1")));
        QVERIFY(!std::any_of(targets.begin(), targets.end(), [](const Target &t) {
            return t.id.contains(QLatin1String("dead"));
        }));
    }

    void envWrappedExecSkipsEnvOptionFlags()
    {
        // `env -i firefox`, `env -u FOO firefox`, `env -C / firefox`,
        // `env -- firefox` must all unwrap to firefox — env's own option
        // flags are not the program. Previously `env -i` unwrapped to
        // program=-i, a dead target that could never launch.
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString root = tmp.path();

        QDir().mkpath(root + QStringLiteral("/apps"));
        const QList<QPair<QString, QByteArray>> entries = {
            {QStringLiteral("firefox"), QByteArrayLiteral("Exec=env -i firefox %u\n")},
            {QStringLiteral("firefox2"), QByteArrayLiteral("Exec=env -u MOZ_X11 firefox %u\n")},
            {QStringLiteral("firefox3"), QByteArrayLiteral("Exec=env --chdir=/ MOZ_X11=1 firefox %u\n")},
            {QStringLiteral("firefox4"), QByteArrayLiteral("Exec=env -- MOZ_X11=1 firefox %u\n")},
            {QStringLiteral("dead"), QByteArrayLiteral("Exec=env -i\n")},
        };
        for (const auto &[name, exec] : entries) {
            QFile desktop(root + QStringLiteral("/apps/%1.desktop").arg(name));
            QVERIFY(desktop.open(QIODevice::WriteOnly));
            desktop.write(QByteArrayLiteral(
                "[Desktop Entry]\n"
                "Name=Firefox\n"
                "Icon=firefox\n"
                "Type=Application\n"
                "Categories=Network;WebBrowser;\n"
                "MimeType=x-scheme-handler/http;x-scheme-handler/https;\n").insert(16, exec));
            desktop.close();
        }

        copyTree(fixtureRoot() + QStringLiteral("/gecko/firefox"),
                  root + QStringLiteral("/config/mozilla/firefox"));

        DiscoveryPaths p;
        p.home = root + QStringLiteral("/home");
        p.configHome = root + QStringLiteral("/config");
        p.dataHome = root + QStringLiteral("/data");
        p.applicationDirs = {root + QStringLiteral("/apps")};

        const auto targets = discoverTargets(p);
        QVERIFY(!std::any_of(targets.begin(), targets.end(), [](const Target &t) {
            return t.exec.startsWith(QLatin1Char('-')) || t.exec == QLatin1String("env")
                || t.id.contains(QLatin1String("dead"));
        }));
        QVERIFY(std::any_of(targets.begin(), targets.end(), [](const Target &t) {
            return t.exec == QLatin1String("firefox");
        }));
    }

    void execPrefixHonorsQuotedArgs()
    {
        // A quoted argument in Exec= must survive as one argv token:
        // --command="zen browser" is a single flatpak argument, not two.
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString root = tmp.path();

        QDir().mkpath(root + QStringLiteral("/apps"));
        QFile desktop(root + QStringLiteral("/apps/app.zen_browser.zen.desktop"));
        QVERIFY(desktop.open(QIODevice::WriteOnly));
        desktop.write(QByteArrayLiteral(
            "[Desktop Entry]\n"
            "Name=Zen Browser\n"
            "Exec=/usr/bin/flatpak run --command=\"zen browser\" app.zen_browser.zen %u\n"
            "Icon=app.zen_browser.zen\n"
            "Type=Application\n"
            "Categories=Network;WebBrowser;\n"
            "MimeType=x-scheme-handler/http;x-scheme-handler/https;\n"));
        desktop.close();

        copyTree(fixtureRoot() + QStringLiteral("/gecko/zen"),
                  root + QStringLiteral("/home/.var/app/app.zen_browser.zen/.zen"));

        DiscoveryPaths p;
        p.home = root + QStringLiteral("/home");
        p.configHome = root + QStringLiteral("/config");
        p.dataHome = root + QStringLiteral("/data");
        p.applicationDirs = {root + QStringLiteral("/apps")};

        const auto targets = discoverTargets(p);
        const auto zenDefault = std::find_if(targets.begin(), targets.end(), [](const Target &t) {
            return t.browserName == QLatin1String("Zen") && t.isBrowserDefault && !t.incognito;
        });
        QVERIFY(zenDefault != targets.end());
        QVERIFY(zenDefault->args.contains(QStringLiteral("--command=zen browser")));
        QVERIFY(!zenDefault->args.contains(QStringLiteral("browser\"")));
        QVERIFY(zenDefault->args.contains(QStringLiteral("app.zen_browser.zen")));
    }

    void chromiumSkipsMissingProfileDirs()
    {
        // Regression: info_cache entries whose profile directory no longer
        // exists must not become dead targets. "Default" is exempt because
        // chromium creates it on first launch.
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString root = tmp.path();

        QDir().mkpath(root + QStringLiteral("/apps"));
        QFile desktop(root + QStringLiteral("/apps/brave-browser.desktop"));
        QVERIFY(desktop.open(QIODevice::WriteOnly));
        desktop.write(QByteArrayLiteral(
            "[Desktop Entry]\n"
            "Name=Brave Browser\n"
            "Exec=/usr/bin/brave %u\n"
            "Icon=brave\n"
            "Type=Application\n"
            "Categories=Network;WebBrowser;\n"
            "MimeType=x-scheme-handler/http;x-scheme-handler/https;\n"));
        desktop.close();

        const QString dataDir = root + QStringLiteral("/config/BraveSoftware/Brave-Browser");
        QDir().mkpath(dataDir);
        QFile localState(dataDir + QStringLiteral("/Local State"));
        QVERIFY(localState.open(QIODevice::WriteOnly));
        localState.write(QByteArrayLiteral(
            "{\"profile\":{\"info_cache\":{"
            "\"Default\":{\"name\":\"Person 1\"},"
            "\"Profile 9\":{\"name\":\"Ghost\"}"
            "}}}"));
        localState.close();

        DiscoveryPaths p;
        p.home = root + QStringLiteral("/home");
        p.configHome = root + QStringLiteral("/config");
        p.dataHome = root + QStringLiteral("/data");
        p.applicationDirs = {root + QStringLiteral("/apps")};

        const auto targets = discoverTargets(p);
        QVERIFY(std::any_of(targets.begin(), targets.end(), [](const Target &t) {
            return t.browserName == QLatin1String("Brave") && t.profileKey == QLatin1String("Default");
        }));
        QVERIFY(!std::any_of(targets.begin(), targets.end(), [](const Target &t) {
            return t.profileKey == QLatin1String("Profile 9") || t.name == QLatin1String("Ghost");
        }));
    }
};

QTEST_MAIN(DiscoveryTest)
#include "test_discovery.moc"
