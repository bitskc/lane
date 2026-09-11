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

    void customTargetsDropSymlinkToBlockedInterpreter()
    {
        // Proven bypass: an innocuous-looking absolute exec path that is
        // itself a symlink to a blocked interpreter. A plain basename
        // check on the literal exec string sails straight through this;
        // only resolving and walking the symlink's actual target catches
        // it (see targetFromJson() in config.cpp).
        QTemporaryDir linkDir;
        QVERIFY(linkDir.isValid());
        const QString linkPath = linkDir.filePath(QStringLiteral("totally-a-browser"));
        QVERIFY(QFile::link(QStringLiteral("/bin/sh"), linkPath));

        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("config.json"));

        QJsonObject evil;
        evil[QStringLiteral("id")] = QStringLiteral("custom:evil");
        evil[QStringLiteral("name")] = QStringLiteral("Evil");
        evil[QStringLiteral("exec")] = linkPath;
        evil[QStringLiteral("args")] = QJsonArray{QStringLiteral("-c"), QStringLiteral("echo pwned")};

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

    void customTargetsDropEnvReExecWrapper()
    {
        // Second proven bypass: exec itself is never a blocked
        // interpreter, only a wrapper (env) that re-execs one named in
        // its own args.
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("config.json"));

        QJsonObject evil;
        evil[QStringLiteral("id")] = QStringLiteral("custom:evil");
        evil[QStringLiteral("name")] = QStringLiteral("Evil");
        evil[QStringLiteral("exec")] = QStringLiteral("/usr/bin/env");
        evil[QStringLiteral("args")] = QJsonArray{QStringLiteral("bash"), QStringLiteral("-c"), QStringLiteral("echo pwned")};

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

    void migrateLegacyConfigFreshCopy()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString oldPath = dir.filePath(QStringLiteral("tern/config.json"));
        const QString newPath = dir.filePath(QStringLiteral("lane/config.json"));
        QVERIFY(QDir().mkpath(QFileInfo(oldPath).absolutePath()));
        const QByteArray body = QByteArrayLiteral("{\"pickerPolicy\":\"always\"}");
        QFile old(oldPath);
        QVERIFY(old.open(QIODevice::WriteOnly));
        old.write(body);
        old.close();

        migrateLegacyConfig(oldPath, newPath);

        QVERIFY(QFile::exists(newPath));
        QFile copied(newPath);
        QVERIFY(copied.open(QIODevice::ReadOnly));
        QCOMPARE(copied.readAll(), body);

        // The legacy file is copied, never moved.
        QVERIFY(QFile::exists(oldPath));
        QFile source(oldPath);
        QVERIFY(source.open(QIODevice::ReadOnly));
        QCOMPARE(source.readAll(), body);
    }

    void migrateLegacyConfigIdempotentOnSecondRun()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString oldPath = dir.filePath(QStringLiteral("tern/config.json"));
        const QString newPath = dir.filePath(QStringLiteral("lane/config.json"));
        QVERIFY(QDir().mkpath(QFileInfo(oldPath).absolutePath()));
        QFile old(oldPath);
        QVERIFY(old.open(QIODevice::WriteOnly));
        old.write(QByteArrayLiteral("{\"pickerPolicy\":\"always\"}"));
        old.close();

        migrateLegacyConfig(oldPath, newPath);
        QVERIFY(QFile::exists(newPath));

        // A real save happens at the new location after migration; a
        // second run (every subsequent startup) must never touch it again.
        QFile updated(newPath);
        QVERIFY(updated.open(QIODevice::WriteOnly | QIODevice::Truncate));
        updated.write(QByteArrayLiteral("{\"pickerPolicy\":\"never\"}"));
        updated.close();

        migrateLegacyConfig(oldPath, newPath);

        QFile check(newPath);
        QVERIFY(check.open(QIODevice::ReadOnly));
        QCOMPARE(check.readAll(), QByteArrayLiteral("{\"pickerPolicy\":\"never\"}"));
    }

    void migrateLegacyConfigDestinationExistsWins()
    {
        // If the new-location config already exists for any reason (a
        // fresh save, an earlier migration, ...), migration must not run
        // at all, even though the legacy file exists and differs.
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString oldPath = dir.filePath(QStringLiteral("tern/config.json"));
        const QString newPath = dir.filePath(QStringLiteral("lane/config.json"));
        QVERIFY(QDir().mkpath(QFileInfo(oldPath).absolutePath()));
        QVERIFY(QDir().mkpath(QFileInfo(newPath).absolutePath()));

        QFile old(oldPath);
        QVERIFY(old.open(QIODevice::WriteOnly));
        old.write(QByteArrayLiteral("{\"pickerPolicy\":\"always\"}"));
        old.close();

        QFile fresh(newPath);
        QVERIFY(fresh.open(QIODevice::WriteOnly));
        fresh.write(QByteArrayLiteral("{\"pickerPolicy\":\"never\"}"));
        fresh.close();

        migrateLegacyConfig(oldPath, newPath);

        QFile check(newPath);
        QVERIFY(check.open(QIODevice::ReadOnly));
        QCOMPARE(check.readAll(), QByteArrayLiteral("{\"pickerPolicy\":\"never\"}"));
    }

    void migrateLegacyConfigFailureLeavesSourceIntact()
    {
        // Force mkpath() to fail: a path component of the destination
        // directory already exists as a plain file, not a directory.
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString oldPath = dir.filePath(QStringLiteral("tern/config.json"));
        QVERIFY(QDir().mkpath(QFileInfo(oldPath).absolutePath()));
        const QByteArray body = QByteArrayLiteral("{\"pickerPolicy\":\"always\"}");
        QFile old(oldPath);
        QVERIFY(old.open(QIODevice::WriteOnly));
        old.write(body);
        old.close();

        const QString blocker = dir.filePath(QStringLiteral("blocked"));
        QFile blockerFile(blocker);
        QVERIFY(blockerFile.open(QIODevice::WriteOnly));
        blockerFile.write("not a directory");
        blockerFile.close();
        const QString newPath = blocker + QStringLiteral("/lane/config.json");

        migrateLegacyConfig(oldPath, newPath);

        QVERIFY(!QFile::exists(newPath));
        QVERIFY(QFile::exists(oldPath));
        QFile source(oldPath);
        QVERIFY(source.open(QIODevice::ReadOnly));
        QCOMPARE(source.readAll(), body);
    }
};

QTEST_MAIN(ConfigTest)
#include "test_config.moc"
