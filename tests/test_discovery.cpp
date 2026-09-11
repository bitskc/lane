#include "core/discovery.h"

#include <QDir>
#include <algorithm>
#include <QFile>
#include <QStandardPaths>
#include <QTest>

using namespace Tern;

static QString fixtureRoot()
{
    return QStringLiteral(TERN_FIXTURE_ROOT);
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

        // Map gecko fixtures into configHome
        // tests copy via CMake; we also accept the layout used in this test by
        // pointing configHome at a synthesized tree.
        QDir().mkpath(p.configHome + QStringLiteral("/zen"));
        QDir().mkpath(p.configHome + QStringLiteral("/mozilla/firefox"));
        QDir().mkpath(p.configHome + QStringLiteral("/BraveSoftware/Brave-Browser/Default"));
        QFile::remove(p.configHome + QStringLiteral("/zen/profiles.ini"));
        QFile::copy(fixtureRoot() + QStringLiteral("/gecko/zen/profiles.ini"),
                    p.configHome + QStringLiteral("/zen/profiles.ini"));
        QFile::copy(fixtureRoot() + QStringLiteral("/gecko/firefox/profiles.ini"),
                    p.configHome + QStringLiteral("/mozilla/firefox/profiles.ini"));
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
    }
};

QTEST_MAIN(DiscoveryTest)
#include "test_discovery.moc"
