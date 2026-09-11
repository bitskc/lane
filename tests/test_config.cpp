#include "core/config.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>

using namespace Lane;

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
        Target custom;
        custom.id = QStringLiteral("custom:notes");
        custom.name = QStringLiteral("Notes");
        custom.exec = QStringLiteral("/usr/bin/true");
        custom.args = {QStringLiteral("$url")};
        c.customTargets = {custom};
        QVERIFY(saveConfig(path, c));
        const Config loaded = loadConfig(path);
        QCOMPARE(loaded.preferPwa, false);
        QCOMPARE(loaded.pickerPolicy, PickerPolicy::Always);
        QCOMPARE(loaded.defaultTargetId, QStringLiteral("browser:zen:x"));
        QCOMPARE(loaded.remembered.value(QStringLiteral("github.com")), QStringLiteral("pwa:gh"));
        QCOMPARE(loaded.rules.size(), 1);
        QCOMPARE(loaded.rules[0].pattern, QStringLiteral("slack.com"));
        QCOMPARE(loaded.customTargets.size(), 1);
        QCOMPARE(loaded.customTargets[0].id, QStringLiteral("custom:notes"));
        QCOMPARE(loaded.customTargets[0].exec, QStringLiteral("/usr/bin/true"));
    }

    void customTargetsDropBlockedInterpreter()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("config.json"));

        QJsonObject evil;
        evil[QStringLiteral("id")] = QStringLiteral("custom:evil");
        evil[QStringLiteral("name")] = QStringLiteral("Evil");
        evil[QStringLiteral("exec")] = QStringLiteral("python3");
        evil[QStringLiteral("args")] = QJsonArray{QStringLiteral("-c"), QStringLiteral("print('pwned')")};

        QJsonObject ok;
        ok[QStringLiteral("id")] = QStringLiteral("custom:ok");
        ok[QStringLiteral("name")] = QStringLiteral("Ok");
        ok[QStringLiteral("exec")] = QStringLiteral("/usr/bin/true");

        QJsonObject root;
        root[QStringLiteral("customTargets")] = QJsonArray{evil, ok};

        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(QJsonDocument(root).toJson());
        f.close();

        const Config loaded = loadConfig(path);
        QCOMPARE(loaded.customTargets.size(), 1);
        QCOMPARE(loaded.customTargets.first().id, QStringLiteral("custom:ok"));
    }

    void corruptConfigMovedAsideNotDiscarded()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("config.json"));
        const QByteArray garbage = QByteArrayLiteral("{ this is not valid json ");

        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(garbage);
        f.close();

        const Config loaded = loadConfig(path);
        QCOMPARE(loaded.pickerPolicy, PickerPolicy::NoRule);
        QVERIFY(loaded.rules.isEmpty());

        QVERIFY(!QFile::exists(path));

        QDir d(dir.path());
        const QStringList corrupted = d.entryList({QStringLiteral("config.json.corrupt-*")}, QDir::Files);
        QCOMPARE(corrupted.size(), 1);

        QFile corrupt(dir.filePath(corrupted.first()));
        QVERIFY(corrupt.open(QIODevice::ReadOnly));
        QCOMPARE(corrupt.readAll(), garbage);
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
