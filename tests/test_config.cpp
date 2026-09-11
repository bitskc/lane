#include "core/config.h"

#include <QTemporaryDir>
#include <QTest>

using namespace Tern;

class ConfigTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void roundTrip()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("config.json"));
        Config c;
        c.preferPwa = false;
        c.pickerPolicy = PickerPolicy::Always;
        c.defaultTargetId = QStringLiteral("browser:zen:x");
        c.remembered.insert(QStringLiteral("github.com"), QStringLiteral("pwa:gh"));
        Rule r;
        r.id = QStringLiteral("r1");
        r.pattern = QStringLiteral("slack.com");
        r.scope = MatchScope::Domain;
        r.targetId = QStringLiteral("browser:zen:work");
        c.rules = {r};
        QVERIFY(saveConfig(path, c));
        const Config loaded = loadConfig(path);
        QCOMPARE(loaded.preferPwa, false);
        QCOMPARE(loaded.pickerPolicy, PickerPolicy::Always);
        QCOMPARE(loaded.defaultTargetId, QStringLiteral("browser:zen:x"));
        QCOMPARE(loaded.remembered.value(QStringLiteral("github.com")), QStringLiteral("pwa:gh"));
        QCOMPARE(loaded.rules.size(), 1);
        QCOMPARE(loaded.rules[0].pattern, QStringLiteral("slack.com"));
    }

    void targetOrderAndAliasesRoundTrip()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("config.json"));
        Config c;
        c.targetOrder = {QStringLiteral("browser:zen:work"), QStringLiteral("pwa:gh")};
        c.targetAliases.insert(QStringLiteral("browser:zen:work"), QStringLiteral("Work Browser"));
        c.targetAliases.insert(QStringLiteral("pwa:gh"), QStringLiteral("GitHub"));
        QVERIFY(saveConfig(path, c));
        const Config loaded = loadConfig(path);
        QCOMPARE(loaded.targetOrder, c.targetOrder);
        QCOMPARE(loaded.targetAliases.value(QStringLiteral("browser:zen:work")), QStringLiteral("Work Browser"));
        QCOMPARE(loaded.targetAliases.value(QStringLiteral("pwa:gh")), QStringLiteral("GitHub"));
        QCOMPARE(loaded.targetAliases.size(), 2);
    }
};

QTEST_MAIN(ConfigTest)
#include "test_config.moc"
