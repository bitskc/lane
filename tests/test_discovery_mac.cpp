// Discovery of browsers from macOS .app bundles (task 8.2): every fixture
// under fixtures/mac is an XML Info.plist so the whole test runs on Linux
// CI; only the binary-plist case needs /usr/bin/plutil and takes its
// explicit-skip path on other OSes.

#include "core/config.h"
#include "core/discovery.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <algorithm>
#include <QTest>

using namespace Lane;

static QString fixtureRoot()
{
    return QStringLiteral(LANE_FIXTURE_ROOT);
}

// Mirrors `src` into `dst`; same helper test_discovery.cpp uses to stage
// profile trees into a synthesized root.
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

class DiscoveryMacTest : public QObject
{
    Q_OBJECT

    // A macOS-shaped DiscoveryPaths: bundles from the fixture roots, the
    // profile-root table's mac side fed from a staged
    // ~/Library/Application Support, and no desktop-file dirs.
    DiscoveryPaths stage(const QString &root)
    {
        const QString support = root + QStringLiteral("/appsupport");
        copyTree(fixtureRoot() + QStringLiteral("/gecko/zen"), support + QStringLiteral("/zen"));
        copyTree(fixtureRoot() + QStringLiteral("/gecko/firefox"), support + QStringLiteral("/Floorp"));
        for (const QString &rel : {QStringLiteral("BraveSoftware/Brave-Browser"),
                                   QStringLiteral("BraveSoftware/Brave-Browser-Beta"),
                                   QStringLiteral("Microsoft Edge")}) {
            const QString dir = support + QLatin1Char('/') + rel;
            QDir().mkpath(dir + QStringLiteral("/Default"));
            QFile::remove(dir + QStringLiteral("/Local State"));
            QFile::copy(fixtureRoot() + QStringLiteral("/chromium/BraveSoftware/Brave-Browser/Local State"),
                        dir + QStringLiteral("/Local State"));
        }

        DiscoveryPaths p;
        p.home = root + QStringLiteral("/home");
        p.configHome = root + QStringLiteral("/config");
        p.dataHome = root + QStringLiteral("/data");
        p.applicationDirs = {};
        p.bundleDirs = {fixtureRoot() + QStringLiteral("/mac"), fixtureRoot() + QStringLiteral("/mac-user")};
        p.applicationSupport = support;
        return p;
    }

private Q_SLOTS:
    void findsBrowsersAndDropsNonBrowsers()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const auto targets = discoverTargets(stage(tmp.path()));
        QStringList ids;
        for (const auto &t : targets) {
            ids << t.id;
        }

        QVERIFY(ids.contains(QStringLiteral("browser:macapp:com.brave.Browser:Default")));
        QVERIFY(ids.contains(QStringLiteral("browser:macapp:com.brave.Browser.beta:Default")));
        QVERIFY(ids.contains(QStringLiteral("browser:macapp:com.microsoft.edgemac:Default")));
        QVERIFY(ids.contains(QStringLiteral("browser:macapp:com.apple.Safari:default")));
        QVERIFY(ids.contains(QStringLiteral("browser:macapp:app.zen-browser.zen:sahyoxd1.Default (release)")));
        QVERIFY(ids.contains(QStringLiteral("browser:macapp:org.mozilla.floorp:x9sh86ht.default")));

        // A bundle is a browser only if a CFBundleURLTypes entry declares
        // http or https: no CFBundleURLTypes at all (Perplexity), a
        // mailto-only URL type (Mail Only), and Lane's own bundle are all
        // excluded.
        QVERIFY(!std::any_of(targets.begin(), targets.end(), [](const Target &t) {
            return t.bundleId == QLatin1String("ai.perplexity.macv3");
        }));
        QVERIFY(!std::any_of(targets.begin(), targets.end(), [](const Target &t) {
            return t.bundleId == QLatin1String("com.example.mailonly");
        }));
        QVERIFY(!ids.filter(QStringLiteral("app.lane")).size());

        // Every mac target carries its bundle fields and the bundle icon
        // key for the image://laneicon/ provider.
        for (const auto &t : targets) {
            if (t.bundleId.isEmpty()) {
                continue;
            }
            QVERIFY(!t.bundlePath.isEmpty());
            QVERIFY(t.bundlePath.endsWith(QLatin1String(".app")));
            QCOMPARE(t.icon, QStringLiteral("bundle:") + t.bundleId);
            QVERIFY(t.id.startsWith(QStringLiteral("browser:macapp:") + t.bundleId + QLatin1Char(':')));
            QVERIFY(t.exec.startsWith(t.bundlePath + QStringLiteral("/Contents/MacOS/")));
            QCOMPARE(macBundlePathForId(t.bundleId), t.bundlePath);
        }
    }

    void dedupesByBundleIdAndRealPath()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const auto targets = discoverTargets(stage(tmp.path()));

        // "Zen Browser.app" exists in both fixture roots (system and user
        // Applications); the same bundle id yields one set of targets, and
        // the shorter bundle path wins.
        const qsizetype zenProfiles = std::count_if(targets.begin(), targets.end(), [](const Target &t) {
            return t.bundleId == QLatin1String("app.zen-browser.zen") && t.kind == Kind::BrowserProfile
                && !t.incognito;
        });
        QCOMPARE(zenProfiles, 3);
        const auto zen = std::find_if(targets.begin(), targets.end(), [](const Target &t) {
            return t.bundleId == QLatin1String("app.zen-browser.zen") && !t.incognito;
        });
        QVERIFY(zen != targets.end());
        QVERIFY(zen->bundlePath.startsWith(fixtureRoot() + QStringLiteral("/mac/")));

        // A symlink to an installed bundle is the same app by resolved
        // real path and adds nothing.
        const QString linkDir = tmp.path() + QStringLiteral("/links");
        QDir().mkpath(linkDir);
        QVERIFY(QFile::link(fixtureRoot() + QStringLiteral("/mac/Brave Browser.app"),
                            linkDir + QStringLiteral("/Brave Alias.app")));
        DiscoveryPaths p = stage(tmp.path());
        p.bundleDirs.append(linkDir);
        const auto viaLink = discoverTargets(p);
        const qsizetype braveProfiles = std::count_if(viaLink.begin(), viaLink.end(), [](const Target &t) {
            return t.bundleId == QLatin1String("com.brave.Browser") && !t.incognito;
        });
        QCOMPARE(braveProfiles, 1);
        QVERIFY(!std::any_of(viaLink.begin(), viaLink.end(), [](const Target &t) {
            return t.bundlePath.contains(QLatin1String("Brave Alias"));
        }));
    }

    void movedBundleKeepsItsIdentity()
    {
        // Renaming or moving a .app must not change the target ids, so
        // rules and remembered choices keyed on the bundle id survive.
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const auto before = discoverTargets(stage(tmp.path()));
        const QStringList beforeIds = [&before] {
            QStringList out;
            for (const auto &t : before) {
                if (t.bundleId == QLatin1String("app.zen-browser.zen")) {
                    out << t.id;
                }
            }
            return out;
        }();
        QVERIFY(!beforeIds.isEmpty());

        QTemporaryDir movedTmp;
        QVERIFY(movedTmp.isValid());
        copyTree(fixtureRoot() + QStringLiteral("/mac/Zen Browser.app"),
                 movedTmp.path() + QStringLiteral("/Totally Renamed.app"));
        DiscoveryPaths p = stage(tmp.path());
        p.bundleDirs = {movedTmp.path()};
        const auto after = discoverTargets(p);
        const QStringList afterIds = [&after] {
            QStringList out;
            for (const auto &t : after) {
                if (t.bundleId == QLatin1String("app.zen-browser.zen")) {
                    out << t.id;
                }
            }
            return out;
        }();
        QCOMPARE(afterIds, beforeIds);
    }

    void rejectsExecutableEscapes()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const auto targets = discoverTargets(stage(tmp.path()));

        // CFBundleExecutable "../..../usr/bin/curl" is not a bare file
        // name; a Contents/MacOS symlink resolving outside the bundle and
        // a name with no file behind it fail the containment check too.
        for (const char *id : {"com.example.badescape", "com.example.symlinkescape",
                               "com.example.missingexec"}) {
            QVERIFY2(!std::any_of(targets.begin(), targets.end(),
                                  [id](const Target &t) { return t.bundleId == QLatin1String(id); }),
                     id);
        }

        // A kept bundle's exec is the literal path inside its own
        // Contents/MacOS, which is what the launcher runs on macOS.
        const auto brave = std::find_if(targets.begin(), targets.end(), [](const Target &t) {
            return t.bundleId == QLatin1String("com.brave.Browser") && !t.incognito;
        });
        QVERIFY(brave != targets.end());
        QCOMPARE(brave->exec, brave->bundlePath + QStringLiteral("/Contents/MacOS/Brave Browser"));
    }

    void profileRootsAndPrivateFlags()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString support = tmp.path() + QStringLiteral("/appsupport");
        const auto targets = discoverTargets(stage(tmp.path()));

        auto onlyTarget = [&](const QString &bundleId, bool incognito) {
            return std::find_if(targets.begin(), targets.end(), [&](const Target &t) {
                return t.bundleId == bundleId && t.incognito == incognito;
            });
        };

        // Chromium roots: Local State profile "Default" (named Personal)
        // under each brand's Application Support directory.
        const auto brave = onlyTarget(QStringLiteral("com.brave.Browser"), false);
        QVERIFY(brave != targets.end());
        QCOMPARE(brave->browserName, QStringLiteral("Brave"));
        QCOMPARE(brave->name, QStringLiteral("Personal"));
        QCOMPARE(brave->profileDir,
                 support + QStringLiteral("/BraveSoftware/Brave-Browser/Default"));
        QCOMPARE(brave->engine, Engine::Chromium);

        const auto braveBeta = onlyTarget(QStringLiteral("com.brave.Browser.beta"), false);
        QVERIFY(braveBeta != targets.end());
        QCOMPARE(braveBeta->browserName, QStringLiteral("Brave Beta"));
        QCOMPARE(braveBeta->profileDir,
                 support + QStringLiteral("/BraveSoftware/Brave-Browser-Beta/Default"));

        const auto edge = onlyTarget(QStringLiteral("com.microsoft.edgemac"), false);
        QVERIFY(edge != targets.end());
        QCOMPARE(edge->profileDir, support + QStringLiteral("/Microsoft Edge/Default"));

        // Edge's private flag on macOS is --inprivate; other Chromium
        // bundles keep --incognito (verified on the port machine: Edge
        // opens a normal window for --incognito).
        const auto edgeInc = onlyTarget(QStringLiteral("com.microsoft.edgemac"), true);
        QVERIFY(edgeInc != targets.end());
        QVERIFY(edgeInc->id.endsWith(QLatin1String(":incognito")));
        QVERIFY(edgeInc->args.contains(QStringLiteral("--inprivate")));
        QVERIFY(!edgeInc->args.contains(QStringLiteral("--incognito")));
        const auto braveInc = onlyTarget(QStringLiteral("com.brave.Browser"), true);
        QVERIFY(braveInc != targets.end());
        QVERIFY(braveInc->args.contains(QStringLiteral("--incognito")));

        // Gecko roots: profiles.ini under Application Support/zen and
        // /Floorp produce the same target suffixes as Linux, including
        // :private variants and :container:<n> for Zen's Work container.
        const auto zenDefault = std::find_if(targets.begin(), targets.end(), [&](const Target &t) {
            return t.bundleId == QLatin1String("app.zen-browser.zen") && t.isBrowserDefault;
        });
        QVERIFY(zenDefault != targets.end());
        QCOMPARE(zenDefault->engine, Engine::Gecko);
        QCOMPARE(zenDefault->profileDir, support + QStringLiteral("/zen/sahyoxd1.Default (release)"));
        QVERIFY(std::any_of(targets.begin(), targets.end(), [](const Target &t) {
            return t.bundleId == QLatin1String("app.zen-browser.zen") && t.incognito
                && t.id.endsWith(QLatin1String(":private"))
                && t.args.contains(QStringLiteral("--private-window"));
        }));
        QVERIFY(std::any_of(targets.begin(), targets.end(), [](const Target &t) {
            return t.bundleId == QLatin1String("app.zen-browser.zen") && t.kind == Kind::Container
                && t.containerName == QLatin1String("Work");
        }));
        const auto floorp = onlyTarget(QStringLiteral("org.mozilla.floorp"), false);
        QVERIFY(floorp != targets.end());
        QVERIFY(floorp->profileDir.startsWith(support + QStringLiteral("/Floorp/")));

        // Safari is a single Generic target: no profile or private
        // variants are invented for it.
        const qsizetype safariTargets = std::count_if(targets.begin(), targets.end(), [](const Target &t) {
            return t.bundleId == QLatin1String("com.apple.Safari");
        });
        QCOMPARE(safariTargets, 1);
        const auto safari = onlyTarget(QStringLiteral("com.apple.Safari"), false);
        QVERIFY(safari != targets.end());
        QCOMPARE(safari->engine, Engine::Generic);
        QVERIFY(safari->args.contains(QStringLiteral("$url")));
    }

    void pwasKeepSameIdsOnMac()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        DiscoveryPaths p = stage(tmp.path());
        copyTree(fixtureRoot() + QStringLiteral("/pwa/firefoxpwa"),
                 p.dataHome + QStringLiteral("/firefoxpwa"));
        const auto targets = discoverTargets(p);
        const qsizetype pwas = std::count_if(targets.begin(), targets.end(), [](const Target &t) {
            return t.kind == Kind::Pwa;
        });
        QCOMPARE(pwas, 3);
        QVERIFY(std::any_of(targets.begin(), targets.end(), [](const Target &t) {
            return t.id == QLatin1String("pwa:01KV62HYP6YRMPPPBMVTRA23HX");
        }));
    }

    void bundleFieldsStayOutOfSavedConfig()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const auto targets = discoverTargets(stage(tmp.path()));
        QVERIFY(std::any_of(targets.begin(), targets.end(), [](const Target &t) {
            return !t.bundleId.isEmpty();
        }));

        // Force a save that includes a mac-app target: only customTargets
        // are serialized, so put one there and check the written JSON
        // carries no bundle fields beyond the target id.
        Config cfg;
        for (const auto &t : targets) {
            if (!t.bundleId.isEmpty() && !t.incognito) {
                cfg.customTargets.append(t);
                break;
            }
        }
        const QString cfgPath = tmp.path() + QStringLiteral("/out/config.json");
        QVERIFY(saveConfig(cfgPath, cfg));
        QFile f(cfgPath);
        QVERIFY(f.open(QIODevice::ReadOnly));
        const QByteArray json = f.readAll();
        QVERIFY(!json.contains("bundleId"));
        QVERIFY(!json.contains("bundlePath"));
        // exec and args are plain target fields that customTargets always
        // serializes; only the bundle-specific fields must stay out.
        QVERIFY(json.contains("browser:macapp:"));
    }

    void binaryPlistIsConvertedOnMac()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const auto targets = discoverTargets(stage(tmp.path()));
        const auto binary = std::find_if(targets.begin(), targets.end(), [](const Target &t) {
            return t.bundleId == QLatin1String("com.example.binary");
        });
        if (!QFile::exists(QStringLiteral("/usr/bin/plutil"))) {
            QSKIP("binary Info.plist conversion needs /usr/bin/plutil (macOS only)");
        }
        QVERIFY(binary != targets.end());
        QCOMPARE(binary->id, QStringLiteral("browser:macapp:com.example.binary:default"));
        QVERIFY(binary->exec.endsWith(QLatin1String("/Binary.app/Contents/MacOS/Binary")));
    }
};

QTEST_MAIN(DiscoveryMacTest)
#include "test_discovery_mac.moc"
