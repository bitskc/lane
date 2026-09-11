#include "router.h"

#include "destination.h"
#include "matcher.h"
#include "urlutil.h"

#include <QSet>

namespace Lane
{

const Target *findTarget(const QList<Target> &targets, const QString &id)
{
    for (const auto &t : targets) {
        if (t.id == id) {
            return &t;
        }
    }
    return nullptr;
}

QStringList danglingRememberedKeys(const QList<Target> &targets, const Config &config)
{
    QStringList out;
    for (auto it = config.remembered.begin(); it != config.remembered.end(); ++it) {
        if (!findTarget(targets, it.value())) {
            out.append(it.key());
        }
    }
    return out;
}

const Target *defaultTarget(const QList<Target> &targets, const Config &config)
{
    if (const Target *t = findTarget(targets, config.defaultTargetId)) {
        if (!t->hidden && t->kind != Kind::Action) {
            return t;
        }
    }
    const Target *browserDefault = nullptr;
    const Target *first = nullptr;
    for (const auto &t : targets) {
        if (t.hidden || t.kind == Kind::Action || t.incognito) {
            continue;
        }
        if (!first) {
            first = &t;
        }
        if (t.isBrowserDefault && !browserDefault) {
            browserDefault = &t;
        }
    }
    return browserDefault ? browserDefault : first;
}

static QList<const Target *> pwaMatches(const Click &click, const QList<Target> &targets)
{
    QList<const Target *> out;
    for (const auto &t : targets) {
        if (t.hidden || t.kind != Kind::Pwa) {
            continue;
        }
        if (urlInScope(click.matchUrl, t.pwaScope)) {
            out.append(&t);
        }
    }
    return out;
}

static QList<QPair<const Rule *, const Target *>> matchingRules(const Click &click, const QList<Target> &targets, const Config &config)
{
    QList<QPair<const Rule *, const Target *>> out;
    for (const auto &rule : config.rules) {
        if (!ruleMatches(rule, click)) {
            continue;
        }
        const Target *t = findTarget(targets, rule.targetId);
        if (t && !t->hidden) {
            out.append({&rule, t});
        }
    }
    return out;
}

QList<Target> rankForPicker(const Click &click, const QList<Target> &targets, const Config &config)
{
    QList<Target> ranked;
    QSet<QString> seen;

    auto push = [&](const Target &t) {
        // action:copy is never a row: it moved to a footer control (see
        // Picker.qml) bound to Controller::copyCurrent(), so it must not
        // consume a row or a number shortcut here regardless of how it
        // would otherwise have been reached (targetOrder pinning included).
        if (t.hidden || t.incognito || seen.contains(t.id) || t.id == QLatin1String("action:copy")) {
            return;
        }
        seen.insert(t.id);
        ranked.append(t);
    };

    for (const auto *t : pwaMatches(click, targets)) {
        push(*t);
    }
    if (const Target *t = findTarget(targets, lookupRemembered(click.matchUrl, config.remembered))) {
        push(*t);
    }
    for (const auto &id : config.targetOrder) {
        if (const Target *t = findTarget(targets, id)) {
            push(*t);
        }
    }
    for (const auto &t : targets) {
        if (t.kind != Kind::Action) {
            push(t);
        }
    }
    return ranked;
}

Decision route(Click click, const QList<Target> &targets, const Config &config)
{
    Decision d;
    d.pickerTargets = rankForPicker(click, targets, config);

    const QString open = click.openUrl.isEmpty() ? click.matchUrl : click.openUrl;
    if (click.forcePicker) {
        d.action = Decision::Action::Pick;
        d.reason = QStringLiteral("forced");
        return d;
    }
    if (!open.isEmpty() && !isSafeOpenUrl(open)) {
        d.action = Decision::Action::Pick;
        d.reason = QStringLiteral("blocked");
        return d;
    }

    const auto rules = matchingRules(click, targets, config);
    if (rules.size() == 1) {
        d.action = Decision::Action::Launch;
        d.target = *rules.front().second;
        d.ruleId = rules.front().first->id;
        d.reason = QStringLiteral("rule");
        return d;
    }
    if (rules.size() > 1) {
        const QString firstId = rules.front().second->id;
        bool conflict = false;
        for (const auto &r : rules) {
            if (r.second->id != firstId) {
                conflict = true;
                break;
            }
        }
        if (!conflict) {
            d.action = Decision::Action::Launch;
            d.target = *rules.front().second;
            d.ruleId = rules.front().first->id;
            d.reason = QStringLiteral("rule");
            return d;
        }
        if (config.pickerPolicy != PickerPolicy::Never) {
            d.action = Decision::Action::Pick;
            d.reason = QStringLiteral("conflict");
            return d;
        }
        // pickerPolicy is Never: the user asked to never see the picker, so
        // a genuine conflict still honors the first matching rule rather
        // than falling through past it.
        d.action = Decision::Action::Launch;
        d.target = *rules.front().second;
        d.ruleId = rules.front().first->id;
        d.reason = QStringLiteral("rule");
        return d;
    }

    d.memoryKey = destinationKeyMatchesBest(click.matchUrl, config.remembered);
    const QString rememberedId = lookupRemembered(click.matchUrl, config.remembered);
    if (const Target *t = findTarget(targets, rememberedId)) {
        if (!t->hidden) {
            d.action = Decision::Action::Launch;
            d.target = *t;
            d.reason = QStringLiteral("remembered");
            return d;
        }
    }

    const auto pwas = pwaMatches(click, targets);
    QList<const Target *> autoPwas;
    for (const auto *t : pwas) {
        if (pwaShouldAutoOpen(*t, click.matchUrl)) {
            autoPwas.append(t);
        }
    }
    if (config.preferPwa && autoPwas.size() == 1) {
        d.action = Decision::Action::Launch;
        d.target = *autoPwas.front();
        d.reason = QStringLiteral("pwa");
        return d;
    }

    switch (config.pickerPolicy) {
    case PickerPolicy::Always:
    case PickerPolicy::NoRule:
        d.action = Decision::Action::Pick;
        d.reason = QStringLiteral("picker");
        return d;
    case PickerPolicy::Conflict:
    case PickerPolicy::Never:
        break;
    }

    if (const Target *t = defaultTarget(targets, config)) {
        d.action = Decision::Action::Launch;
        d.target = *t;
        d.reason = QStringLiteral("default");
        return d;
    }

    d.action = Decision::Action::Pick;
    d.reason = QStringLiteral("empty");
    return d;
}

} // namespace Lane
