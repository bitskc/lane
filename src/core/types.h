#pragma once

#include <QColor>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QList>
#include <QMap>

namespace Lane
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
    Container,
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
    int containerId = 0;
    QString containerName;
    QColor color;
    QString customName;
    bool hidden = false;
    bool isBrowserDefault = false;
    bool incognito = false;
    bool frameless = false;

    QString displayName() const
    {
        if (!customName.isEmpty()) {
            return customName;
        }
        if (kind == Kind::Pwa || kind == Kind::Action || kind == Kind::Custom) {
            return name;
        }
        if (name.isEmpty() || name.compare(browserName, Qt::CaseInsensitive) == 0) {
            return browserName;
        }
        return browserName + QStringLiteral(" · ") + name;
    }

    QString discoveredName() const
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
    // Optional Plasma Activity scope: an Activity ID or name this rule is
    // limited to. Empty means the rule applies in every Activity. Only
    // consulted while activityRoutingEnabled is on and the desktop
    // reports a current Activity.
    QString activity;
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
    // Plasma Activity the click arrived under. activityId carries the
    // Activity UUID and activityName its display name (a rule's activity
    // filter may name either). Both stay empty when
    // activityRoutingEnabled is off, when Lane runs on a non-Plasma
    // desktop, or when the activities service has not answered yet; in
    // every one of those cases routing is identical to Lane without
    // Activity support.
    QString activityId;
    QString activityName;
    // Tracking-parameter keys the clean-links cleaner dropped from
    // matchUrl (see runPipeline() in pipeline.cpp). Empty when
    // stripTrackingParams is off, the URL had no query, or nothing
    // matched the banned-key set.
    QStringList removedTrackingParams;
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
    QString memoryKey;
    QList<Target> pickerTargets;
};

struct Config {
    int version = 1;
    PickerPolicy pickerPolicy = PickerPolicy::NoRule;
    bool closeOnFocusLoss = true;
    bool showUrl = true;
    bool toast = true;
    bool unwrapO365 = true;
    bool unshorten = true;
    bool stripTrackingParams = true;
    bool openUnwrapped = false;
    bool preferPwa = true;
    bool holdAutoOpen = false;
    int holdMs = 1600;
    bool autostart = false;
    QString defaultTargetId;
    QStringList hiddenTargetIds;
    QList<Rule> rules;
    QStringList targetOrder;
    QMap<QString, QString> targetAliases;
    QMap<QString, QString> remembered;
    QList<Target> customTargets;
    // Off by default: Activity-scoped rules and activityDefaults only
    // take effect when the user opts in and a current Plasma Activity is
    // known (the Controller stamps Click::activityId/activityName only
    // while this is on).
    bool activityRoutingEnabled = false;
    // Plasma Activity ID -> target ID fallback. Checked when no rule,
    // remembered destination, or PWA matched, and takes precedence over
    // defaultTargetId only while Activity routing is active.
    QMap<QString, QString> activityDefaults;
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
    case Kind::Container:
        return QStringLiteral("container");
    case Kind::BrowserProfile:
        return QStringLiteral("browser");
    }
    return QStringLiteral("browser");
}

inline QString engineName(Engine e)
{
    switch (e) {
    case Engine::Chromium:
        return QStringLiteral("chromium");
    case Engine::Gecko:
        return QStringLiteral("gecko");
    case Engine::Pwa:
        return QStringLiteral("pwa");
    case Engine::Action:
        return QStringLiteral("action");
    case Engine::Generic:
        return QStringLiteral("generic");
    }
    return QStringLiteral("generic");
}

} // namespace Lane
