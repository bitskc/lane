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
};

QTEST_MAIN(DiscoveryTest)
#include "test_discovery.moc"
