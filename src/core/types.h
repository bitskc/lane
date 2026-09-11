#pragma once

#include <QColor>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QList>
#include <QMap>

namespace Tern
{

enum class Engine {
    Generic,
    Chromium,
    Gecko,
    Pwa,
    Action,
};

enum class Kind {
    BrowserProfile,
    Pwa,
    Custom,
    Action,
};

enum class MatchScope {
    Any,
    Domain,
    Path,
};

enum class MatchLocation {
    Url,
    WindowTitle,
    ProcessName,
};

enum class PickerPolicy {
    Always,
    NoRule,
    Conflict,
    Never,
};

struct Target {
    QString id;
    Kind kind = Kind::BrowserProfile;
    Engine engine = Engine::Generic;
    QString name;
    QString browserName;
    QString subtitle;
    QString exec;
    QStringList args;
    QString icon;
    QString profileKey;
    QString profileDir;
    QString pwaUlid;
    QString pwaScope;
    QColor color;
    bool hidden = false;
    bool isBrowserDefault = false;
    bool incognito = false;
    bool frameless = false;

    QString displayName() const
    {
        if (kind == Kind::Pwa || kind == Kind::Action || kind == Kind::Custom) {
            return name;
        }
        if (name.isEmpty() || name.compare(browserName, Qt::CaseInsensitive) == 0) {
            return browserName;
        }
        return browserName + QStringLiteral(" · ") + name;
    }
};

struct Rule {
    QString id;
    QString pattern;
    MatchScope scope = MatchScope::Domain;
    MatchLocation location = MatchLocation::Url;
    bool regex = false;
    QString targetId;
    bool enabled = true;
};

struct Substitution {
    QString find;
    QString replace;
    bool regex = false;
};

struct Click {
    QString originalUrl;
    QString matchUrl;
    QString openUrl;
    QString host;
    QString path;
    QString processName;
    QString windowTitle;
    bool forcePicker = false;
};

struct Decision {
    enum class Action {
        Launch,
        Pick,
        Copy,
    };
    Action action = Action::Pick;
    Target target;
    QString reason;
    QString ruleId;
    QList<Target> pickerTargets;
};

struct Config {
    int version = 1;
    PickerPolicy pickerPolicy = PickerPolicy::NoRule;
    bool closeOnFocusLoss = true;
    bool showUrl = true;
    bool toast = true;
    int toastMs = 2800;
    bool unwrapO365 = true;
    bool unshorten = true;
    bool openUnwrapped = false;
    bool preferPwa = true;
    bool autostart = false;
    QString defaultTargetId;
    QStringList hiddenTargetIds;
    QList<Rule> rules;
    QMap<QString, QString> remembered;
    QStringList recentTargetIds;
    QList<Target> customTargets;
    QList<Substitution> substitutions;
};

inline QString kindName(Kind k)
{
    switch (k) {
    case Kind::Pwa:
        return QStringLiteral("pwa");
    case Kind::Custom:
        return QStringLiteral("app");
    case Kind::Action:
        return QStringLiteral("action");
    case Kind::BrowserProfile:
        return QStringLiteral("browser");
    }
    return QStringLiteral("browser");
}

} // namespace Tern
