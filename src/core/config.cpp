#include "config.h"

#include "launcher.h"

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QFileInfo>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUuid>

namespace Lane
{

QString pickerPolicyToString(PickerPolicy p)
{
    switch (p) {
    case PickerPolicy::Always:
        return QStringLiteral("always");
    case PickerPolicy::Conflict:
        return QStringLiteral("conflict");
    case PickerPolicy::Never:
        return QStringLiteral("never");
    case PickerPolicy::NoRule:
        break;
    }
    return QStringLiteral("no-rule");
}

PickerPolicy pickerPolicyFromString(const QString &s)
{
    if (s == QLatin1String("always")) {
        return PickerPolicy::Always;
    }
    if (s == QLatin1String("conflict")) {
        return PickerPolicy::Conflict;
    }
    if (s == QLatin1String("never")) {
        return PickerPolicy::Never;
    }
    return PickerPolicy::NoRule;
}

QString scopeToString(MatchScope s)
{
    switch (s) {
    case MatchScope::Domain:
        return QStringLiteral("domain");
    case MatchScope::Path:
        return QStringLiteral("path");
    case MatchScope::Any:
        break;
    }
    return QStringLiteral("any");
}

MatchScope scopeFromString(const QString &s)
{
    if (s == QLatin1String("domain")) {
        return MatchScope::Domain;
    }
    if (s == QLatin1String("path")) {
        return MatchScope::Path;
    }
    return MatchScope::Any;
}

QString locationToString(MatchLocation l)
{
    switch (l) {
    case MatchLocation::WindowTitle:
        return QStringLiteral("title");
    case MatchLocation::ProcessName:
        return QStringLiteral("process");
    case MatchLocation::Url:
        break;
    }
    return QStringLiteral("url");
}

MatchLocation locationFromString(const QString &s)
{
    if (s == QLatin1String("title")) {
        return MatchLocation::WindowTitle;
    }
    if (s == QLatin1String("process")) {
        return MatchLocation::ProcessName;
    }
    return MatchLocation::Url;
}

static QJsonObject targetToJson(const Target &t)
{
    QJsonObject o;
    o[QStringLiteral("id")] = t.id;
    o[QStringLiteral("name")] = t.name;
    o[QStringLiteral("browserName")] = t.browserName;
    o[QStringLiteral("exec")] = t.exec;
    o[QStringLiteral("args")] = QJsonArray::fromStringList(t.args);
    o[QStringLiteral("icon")] = t.icon;
    o[QStringLiteral("kind")] = kindName(t.kind);
    return o;
}

static bool targetFromJson(const QJsonObject &o, Target *out)
{
    Target t;
    t.id = o[QStringLiteral("id")].toString();
    t.name = o[QStringLiteral("name")].toString();
    t.browserName = o[QStringLiteral("browserName")].toString();
    t.exec = o[QStringLiteral("exec")].toString();
    t.icon = o[QStringLiteral("icon")].toString();
    t.kind = Kind::Custom;
    t.engine = Engine::Generic;
    const auto args = o[QStringLiteral("args")].toArray();
    for (const auto &a : args) {
        t.args << a.toString();
    }
    if (t.id.isEmpty()) {
        t.id = QStringLiteral("custom:") + QUuid::createUuid().toString(QUuid::WithoutBraces);
    }
    if (isBlockedInterpreter(t.exec)) {
        qWarning() << "Lane: dropping customTargets entry" << t.id << "because its exec is a blocked shell/interpreter:" << t.exec;
        return false;
    }
    t.subtitle = t.browserName.isEmpty() ? QStringLiteral("App") : t.browserName;
    *out = t;
    return true;
}

QString defaultConfigPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + QStringLiteral("/lane/config.json");
}

static QString legacyConfigPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + QStringLiteral("/tern/config.json");
}

void migrateLegacyConfig()
{
    const QString newPath = defaultConfigPath();
    if (QFile::exists(newPath)) {
        // The current location already has a file, whether from a previous
        // migration or a fresh save. Never overwrite it: this is what makes
        // the migration safe to run on every startup.
        return;
    }
    const QString oldPath = legacyConfigPath();
    if (!QFile::exists(oldPath)) {
        return;
    }
    const QString newDir = QFileInfo(newPath).absolutePath();
    if (!QDir().mkpath(newDir)) {
        qWarning() << "Lane: could not create config directory for migration:" << newDir;
        return;
    }
    // Copy, never move or rename: the legacy file at ~/.config/tern is left
    // byte-for-byte intact no matter what happens here, so a failed or
    // partial migration can never lose the user's rules, remembered
    // destinations, aliases, or target order.
    if (QFile::copy(oldPath, newPath)) {
        qInfo() << "Lane: migrated config from" << oldPath << "to" << newPath;
    } else {
        qWarning() << "Lane: failed to migrate config from" << oldPath << "to" << newPath
                   << "- starting fresh at the new location instead";
    }
}

Config loadConfig(const QString &path)
{
    Config c;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        return c;
    }
    const QByteArray raw = f.readAll();
    f.close();
    const auto doc = QJsonDocument::fromJson(raw);
    if (!doc.isObject()) {
        const QString corruptPath = path + QStringLiteral(".corrupt-")
            + QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMddTHHmmssZ"));
        if (QFile::rename(path, corruptPath)) {
            qWarning() << "Lane: config at" << path << "is not valid JSON; moved it aside to" << corruptPath;
        } else {
            qWarning() << "Lane: config at" << path << "is not valid JSON and could not be moved aside; using defaults";
        }
        return c;
    }
    const QJsonObject o = doc.object();
    c.version = o[QStringLiteral("version")].toInt(1);
    c.pickerPolicy = pickerPolicyFromString(o[QStringLiteral("pickerPolicy")].toString());
    c.closeOnFocusLoss = o[QStringLiteral("closeOnFocusLoss")].toBool(true);
    c.showUrl = o[QStringLiteral("showUrl")].toBool(true);
    c.toast = o[QStringLiteral("toast")].toBool(true);
    c.toastMs = o[QStringLiteral("toastMs")].toInt(2800);
    c.unwrapO365 = o[QStringLiteral("unwrapO365")].toBool(true);
    c.unshorten = o[QStringLiteral("unshorten")].toBool(true);
    c.openUnwrapped = o[QStringLiteral("openUnwrapped")].toBool(false);
    c.preferPwa = o[QStringLiteral("preferPwa")].toBool(true);
    c.holdAutoOpen = o[QStringLiteral("holdAutoOpen")].toBool(true);
    c.holdMs = o[QStringLiteral("holdMs")].toInt(1600);
    c.autostart = o[QStringLiteral("autostart")].toBool(false);
    c.defaultTargetId = o[QStringLiteral("defaultTargetId")].toString();

    for (const auto &v : o[QStringLiteral("hiddenTargetIds")].toArray()) {
        c.hiddenTargetIds << v.toString();
    }
    for (const auto &v : o[QStringLiteral("recentTargetIds")].toArray()) {
        c.recentTargetIds << v.toString();
    }
    for (const auto &v : o[QStringLiteral("targetOrder")].toArray()) {
        c.targetOrder << v.toString();
    }
    const auto aliases = o[QStringLiteral("targetAliases")].toObject();
    for (auto it = aliases.begin(); it != aliases.end(); ++it) {
        c.targetAliases.insert(it.key(), it.value().toString());
    }

    const auto remembered = o[QStringLiteral("remembered")].toObject();
    for (auto it = remembered.begin(); it != remembered.end(); ++it) {
        c.remembered.insert(it.key(), it.value().toString());
    }

    for (const auto &v : o[QStringLiteral("rules")].toArray()) {
        const auto r = v.toObject();
        Rule rule;
        rule.id = r[QStringLiteral("id")].toString();
        if (rule.id.isEmpty()) {
            rule.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        }
        rule.pattern = r[QStringLiteral("pattern")].toString();
        rule.scope = scopeFromString(r[QStringLiteral("scope")].toString());
        rule.location = locationFromString(r[QStringLiteral("location")].toString());
        rule.regex = r[QStringLiteral("regex")].toBool(false);
        rule.targetId = r[QStringLiteral("targetId")].toString();
        rule.enabled = r[QStringLiteral("enabled")].toBool(true);
        c.rules.append(rule);
    }

    for (const auto &v : o[QStringLiteral("customTargets")].toArray()) {
        Target t;
        if (targetFromJson(v.toObject(), &t)) {
            c.customTargets.append(t);
        }
    }

    for (const auto &v : o[QStringLiteral("substitutions")].toArray()) {
        const auto s = v.toObject();
        Substitution sub;
        sub.find = s[QStringLiteral("find")].toString();
        sub.replace = s[QStringLiteral("replace")].toString();
        sub.regex = s[QStringLiteral("regex")].toBool(false);
        c.substitutions.append(sub);
    }
    return c;
}

bool saveConfig(const QString &path, const Config &config)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QJsonObject o;
    o[QStringLiteral("version")] = config.version;
    o[QStringLiteral("pickerPolicy")] = pickerPolicyToString(config.pickerPolicy);
    o[QStringLiteral("closeOnFocusLoss")] = config.closeOnFocusLoss;
    o[QStringLiteral("showUrl")] = config.showUrl;
    o[QStringLiteral("toast")] = config.toast;
    o[QStringLiteral("toastMs")] = config.toastMs;
    o[QStringLiteral("unwrapO365")] = config.unwrapO365;
    o[QStringLiteral("unshorten")] = config.unshorten;
    o[QStringLiteral("openUnwrapped")] = config.openUnwrapped;
    o[QStringLiteral("preferPwa")] = config.preferPwa;
    o[QStringLiteral("holdAutoOpen")] = config.holdAutoOpen;
    o[QStringLiteral("holdMs")] = config.holdMs;
    o[QStringLiteral("autostart")] = config.autostart;
    o[QStringLiteral("defaultTargetId")] = config.defaultTargetId;
    o[QStringLiteral("hiddenTargetIds")] = QJsonArray::fromStringList(config.hiddenTargetIds);
    o[QStringLiteral("recentTargetIds")] = QJsonArray::fromStringList(config.recentTargetIds);
    o[QStringLiteral("targetOrder")] = QJsonArray::fromStringList(config.targetOrder);
    QJsonObject aliases;
    for (auto it = config.targetAliases.begin(); it != config.targetAliases.end(); ++it) {
        aliases.insert(it.key(), it.value());
    }
    o[QStringLiteral("targetAliases")] = aliases;

    QJsonObject remembered;
    for (auto it = config.remembered.begin(); it != config.remembered.end(); ++it) {
        remembered.insert(it.key(), it.value());
    }
    o[QStringLiteral("remembered")] = remembered;

    QJsonArray rules;
    for (const auto &rule : config.rules) {
        QJsonObject r;
        r[QStringLiteral("id")] = rule.id;
        r[QStringLiteral("pattern")] = rule.pattern;
        r[QStringLiteral("scope")] = scopeToString(rule.scope);
        r[QStringLiteral("location")] = locationToString(rule.location);
        r[QStringLiteral("regex")] = rule.regex;
        r[QStringLiteral("targetId")] = rule.targetId;
        r[QStringLiteral("enabled")] = rule.enabled;
        rules.append(r);
    }
    o[QStringLiteral("rules")] = rules;

    QJsonArray customs;
    for (const auto &t : config.customTargets) {
        customs.append(targetToJson(t));
    }
    o[QStringLiteral("customTargets")] = customs;

    QJsonArray subs;
    for (const auto &s : config.substitutions) {
        QJsonObject j;
        j[QStringLiteral("find")] = s.find;
        j[QStringLiteral("replace")] = s.replace;
        j[QStringLiteral("regex")] = s.regex;
        subs.append(j);
    }
    o[QStringLiteral("substitutions")] = subs;

    const QByteArray json = QJsonDocument(o).toJson(QJsonDocument::Indented);
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly)) {
        return false;
    }
    if (f.write(json) != json.size()) {
        f.cancelWriting();
        return false;
    }
    return f.commit();
}

} // namespace Lane
