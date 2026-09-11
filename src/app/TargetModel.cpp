#include "TargetModel.h"

namespace Tern
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
    case IncognitoRole:
        return t.incognito;
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
        {IncognitoRole, "incognito"},
    };
}

void TargetModel::setTargets(QList<Target> targets)
{
    beginResetModel();
    m_targets = std::move(targets);
    endResetModel();
}

QString TargetModel::idAt(int row) const
{
    if (row < 0 || row >= m_targets.size()) {
        return {};
    }
    return m_targets.at(row).id;
}

} // namespace Tern
