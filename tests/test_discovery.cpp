#include "core/discovery.h"

#include <QDir>
#include <QFileInfo>
#include <QTemporaryDir>
#include <algorithm>
#include <QFile>
#include <QStandardPaths>
#include <QTest>

using namespace Tern;

static QString fixtureRoot()
{
    return QStringLiteral(TERN_FIXTURE_ROOT);
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
        QVERIFY(!ids.filter(QStringLiteral("app.tern")).size());

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
};

QTEST_MAIN(DiscoveryTest)
#include "test_discovery.moc"
