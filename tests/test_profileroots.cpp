// The per-OS profile-root table (task 8.1): with an empty
// applicationSupport the Linux candidate lists resolve exactly where they
// always did, and with applicationSupport set the macOS candidates under
// ~/Library/Application Support win instead. Runs on Linux; the bundle
// fixtures carry no macOS dependency.

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

class ProfileRootsTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void linuxRootsAreUnchanged()
    {
        // The same staging test_discovery uses; Linux rows must keep
        // resolving under configHome and home, never applicationSupport
        // (which stays empty), and bundle-free runs must not grow
        // browser:macapp: ids.
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString root = tmp.path();

        copyTree(fixtureRoot() + QStringLiteral("/gecko/zen"),
                 root + QStringLiteral("/config/zen"));
        copyTree(fixtureRoot() + QStringLiteral("/gecko/firefox"),
                 root + QStringLiteral("/config/mozilla/firefox"));
        const QString braveDir = root + QStringLiteral("/config/BraveSoftware/Brave-Browser");
        QDir().mkpath(braveDir + QStringLiteral("/Default"));
        QFile::copy(fixtureRoot() + QStringLiteral("/chromium/BraveSoftware/Brave-Browser/Local State"),
                    braveDir + QStringLiteral("/Local State"));

        DiscoveryPaths p;
        p.home = root + QStringLiteral("/home");
        p.configHome = root + QStringLiteral("/config");
        p.dataHome = root + QStringLiteral("/data");
        p.applicationDirs = {fixtureRoot() + QStringLiteral("/desktop")};

        const auto targets = discoverTargets(p);
        QVERIFY(std::all_of(targets.begin(), targets.end(), [](const Target &t) {
            return t.bundleId.isEmpty() && !t.id.startsWith(QLatin1String("browser:macapp:"));
        }));

        const auto zenDefault = std::find_if(targets.begin(), targets.end(), [](const Target &t) {
            return t.browserName == QLatin1String("Zen") && t.isBrowserDefault && !t.incognito;
        });
        QVERIFY(zenDefault != targets.end());
        QCOMPARE(zenDefault->profileDir,
                 root + QStringLiteral("/config/zen/sahyoxd1.Default (release)"));

        const auto firefoxDefault = std::find_if(targets.begin(), targets.end(), [](const Target &t) {
            return t.browserName == QLatin1String("Firefox") && t.isBrowserDefault && !t.incognito;
        });
        QVERIFY(firefoxDefault != targets.end());
        QCOMPARE(firefoxDefault->profileDir,
                 root + QStringLiteral("/config/mozilla/firefox/x9sh86ht.default"));

        const auto brave = std::find_if(targets.begin(), targets.end(), [](const Target &t) {
            return t.browserName == QLatin1String("Brave") && !t.incognito;
        });
        QVERIFY(brave != targets.end());
        QCOMPARE(brave->profileDir,
                 root + QStringLiteral("/config/BraveSoftware/Brave-Browser/Default"));

        // No discovery root outside the staged configHome/home may leak
        // into a profile path.
        QVERIFY(!std::any_of(targets.begin(), targets.end(), [&root](const Target &t) {
            return !t.profileDir.isEmpty() && !t.profileDir.startsWith(root);
        }));
    }

    void linuxFlatpakRootsStillWork()
    {
        // The flatpakRel entries of the linux side of the table still feed
        // ~/.var/app lookups; covered end-to-end by test_discovery's
        // Flatpak cases, pinned here on the root itself.
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString root = tmp.path();

        QDir().mkpath(root + QStringLiteral("/apps"));
        QFile desktop(root + QStringLiteral("/apps/app.zen_browser.zen.desktop"));
        QVERIFY(desktop.open(QIODevice::WriteOnly));
        desktop.write(QByteArrayLiteral(
            "[Desktop Entry]\n"
            "Name=Zen Browser\n"
            "Exec=/usr/bin/flatpak run --command=zen app.zen_browser.zen %u\n"
            "Type=Application\n"
            "Categories=Network;WebBrowser;\n"
            "MimeType=x-scheme-handler/http;\n"));
        desktop.close();
        copyTree(fixtureRoot() + QStringLiteral("/gecko/zen"),
                 root + QStringLiteral("/home/.var/app/app.zen_browser.zen/.zen"));

        DiscoveryPaths p;
        p.home = root + QStringLiteral("/home");
        p.configHome = root + QStringLiteral("/config");
        p.dataHome = root + QStringLiteral("/data");
        p.applicationDirs = {root + QStringLiteral("/apps")};

        const auto targets = discoverTargets(p);
        const auto zen = std::find_if(targets.begin(), targets.end(), [](const Target &t) {
            return t.browserName == QLatin1String("Zen") && !t.incognito;
        });
        QVERIFY(zen != targets.end());
        QVERIFY(zen->profileDir.startsWith(root + QStringLiteral("/home/.var/app/app.zen_browser.zen/.zen/")));
    }

    void macRootsYieldApplicationSupportDirs()
    {
        // With applicationSupport set (what defaultDiscoveryPaths() does
        // on macOS) the mac column of the table drives profile lookup:
        // Gecko under Application Support/<App>, Chromium under
        // Application Support/<Vendor dir>.
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString root = tmp.path();
        const QString support = root + QStringLiteral("/appsupport");

        copyTree(fixtureRoot() + QStringLiteral("/gecko/zen"), support + QStringLiteral("/zen"));
        copyTree(fixtureRoot() + QStringLiteral("/gecko/firefox"), support + QStringLiteral("/Floorp"));
        for (const QString &rel : {QStringLiteral("BraveSoftware/Brave-Browser"),
                                   QStringLiteral("BraveSoftware/Brave-Browser-Beta"),
                                   QStringLiteral("Microsoft Edge")}) {
            const QString dir = support + QLatin1Char('/') + rel;
            QDir().mkpath(dir + QStringLiteral("/Default"));
            QFile::copy(fixtureRoot() + QStringLiteral("/chromium/BraveSoftware/Brave-Browser/Local State"),
                        dir + QStringLiteral("/Local State"));
        }

        DiscoveryPaths p;
        p.home = root + QStringLiteral("/home");
        p.configHome = root + QStringLiteral("/config");
        p.dataHome = root + QStringLiteral("/data");
        p.applicationDirs = {};
        p.bundleDirs = {fixtureRoot() + QStringLiteral("/mac")};
        p.applicationSupport = support;

        const auto targets = discoverTargets(p);
        const auto profileDirOf = [&](const QString &bundleId) {
            const auto it = std::find_if(targets.begin(), targets.end(), [&](const Target &t) {
                return t.bundleId == bundleId && !t.incognito;
            });
            return it == targets.end() ? QString() : it->profileDir;
        };

        QVERIFY(profileDirOf(QStringLiteral("app.zen-browser.zen"))
                    .startsWith(support + QStringLiteral("/zen/")));
        QVERIFY(profileDirOf(QStringLiteral("org.mozilla.floorp"))
                    .startsWith(support + QStringLiteral("/Floorp/")));
        QCOMPARE(profileDirOf(QStringLiteral("com.brave.Browser")),
                 support + QStringLiteral("/BraveSoftware/Brave-Browser/Default"));
        QCOMPARE(profileDirOf(QStringLiteral("com.brave.Browser.beta")),
                 support + QStringLiteral("/BraveSoftware/Brave-Browser-Beta/Default"));
        QCOMPARE(profileDirOf(QStringLiteral("com.microsoft.edgemac")),
                 support + QStringLiteral("/Microsoft Edge/Default"));

        // Nothing under the linux-side roots leaks into a macOS run.
        QVERIFY(!std::any_of(targets.begin(), targets.end(), [&root](const Target &t) {
            return t.profileDir.startsWith(root + QStringLiteral("/config/"));
        }));

        // Gecko container and firefoxpwa fixtures produce the same
        // identity suffixes as on Linux.
        QVERIFY(std::any_of(targets.begin(), targets.end(), [](const Target &t) {
            return t.bundleId == QLatin1String("app.zen-browser.zen")
                && t.id.contains(QLatin1String(":container:"));
        }));
        copyTree(fixtureRoot() + QStringLiteral("/pwa/firefoxpwa"),
                 p.dataHome + QStringLiteral("/firefoxpwa"));
        const auto withPwas = discoverTargets(p);
        QVERIFY(std::any_of(withPwas.begin(), withPwas.end(), [](const Target &t) {
            return t.id == QLatin1String("pwa:01KV62HYP6YRMPPPBMVTRA23HX");
        }));

        // Both Application Support/zen and /Zen are candidates (the
        // case-insensitive volume case). One profile store is walked.
        copyTree(support + QStringLiteral("/zen"), support + QStringLiteral("/Zen"));
        const auto withBoth = discoverTargets(p);
        const qsizetype zenProfiles = std::count_if(withBoth.begin(), withBoth.end(), [](const Target &t) {
            return t.bundleId == QLatin1String("app.zen-browser.zen") && t.kind == Kind::BrowserProfile
                && !t.incognito;
        });
        QCOMPARE(zenProfiles, 3);
    }
};

QTEST_MAIN(ProfileRootsTest)
#include "test_profileroots.moc"
