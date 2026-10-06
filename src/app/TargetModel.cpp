#include "TargetModel.h"

#include <algorithm>
#include <limits>

namespace Lane
{

TargetModel::TargetModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int TargetModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_targets.size();
}

QVariant TargetModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_targets.size()) {
        return {};
    }
    const auto &t = m_targets.at(index.row());
    switch (role) {
    case IdRole:
        return t.id;
    case NameRole:
        return t.displayName();
    case SubtitleRole:
        return t.id;
    case IconRole:
        return t.icon;
    case KindRole:
        return kindName(t.kind);
    case HiddenRole:
        return t.hidden;
    case DefaultRole:
        return t.isBrowserDefault;
    case DiscoveredNameRole:
        return t.discoveredName();
    default:
        return {};
    }
}

QHash<int, QByteArray> TargetModel::roleNames() const
{
    return {
        {IdRole, "targetId"},
        {NameRole, "name"},
        {SubtitleRole, "subtitle"},
        {IconRole, "iconName"},
        {KindRole, "kind"},
        {HiddenRole, "hidden"},
        {DefaultRole, "isDefault"},
        {DiscoveredNameRole, "discoveredName"},
    };
}

void TargetModel::setTargets(QList<Target> targets)
{
    beginResetModel();
    m_targets = std::move(targets);
    endResetModel();
}

QVariantList TargetModel::orderableTargets() const
{
    // The same set moveIdAmongSiblings() reorders: every non-incognito,
    // non-Action target, in current (config-ordered) position, so the
    // Settings list and the picker agree on one global order.
    QVariantList out;
    for (const auto &t : m_targets) {
        if (t.incognito || t.kind == Kind::Action) {
            continue;
        }
        QVariantMap m;
        m[QStringLiteral("targetId")] = t.id;
        m[QStringLiteral("name")] = t.displayName();
        m[QStringLiteral("discoveredName")] = t.discoveredName();
        m[QStringLiteral("iconName")] = t.icon;
        m[QStringLiteral("kind")] = kindName(t.kind);
        m[QStringLiteral("hidden")] = t.hidden;
        m[QStringLiteral("isDefault")] = t.isBrowserDefault;
        m[QStringLiteral("engine")] = engineName(t.engine);
        out.append(m);
    }
    return out;
}

QVariantList TargetModel::incognitoTargets() const
{
    // The Private windows list mirrors the user's Destinations order
    // instead of m_targets' raw array order: landmark slots freeze when
    // siblings are reordered (an incognito row keeps the slot it was
    // discovered at while browsers move past it), so the parent id —
    // incognito ids are always <parent>:private or <parent>:incognito —
    // maps each row to its profile's rank among orderable targets.
    QHash<QString, int> orderRank;
    int rank = 0;
    for (const auto &t : m_targets) {
        if (!t.incognito && t.kind != Kind::Action) {
            orderRank.insert(t.id, rank++);
        }
    }

    QList<const Target *> incognitos;
    for (const auto &t : m_targets) {
        if (t.incognito) {
            incognitos.append(&t);
        }
    }
    std::sort(incognitos.begin(), incognitos.end(), [&orderRank](const Target *a, const Target *b) {
        const auto parentRank = [&orderRank](const Target *t) {
            QString id = t->id;
            if (id.endsWith(QLatin1String(":private"))) {
                id.chop(8);
            } else if (id.endsWith(QLatin1String(":incognito"))) {
                id.chop(10);
            }
            return orderRank.value(id, std::numeric_limits<int>::max());
        };
        return parentRank(a) < parentRank(b);
    });

    QVariantList out;
    for (const auto *t : incognitos) {
        QVariantMap m;
        m[QStringLiteral("targetId")] = t->id;
        m[QStringLiteral("name")] = t->displayName();
        m[QStringLiteral("discoveredName")] = t->discoveredName();
        m[QStringLiteral("iconName")] = t->icon;
        m[QStringLiteral("hidden")] = t->hidden;
        out.append(m);
    }
    return out;
}

} // namespace Lane
