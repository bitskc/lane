#include "PickerModel.h"

namespace Tern
{

PickerModel::PickerModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int PickerModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_shown.size();
}

QVariant PickerModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_shown.size()) {
        return {};
    }
    const auto &t = m_shown.at(index.row());
    switch (role) {
    case IdRole:
        return t.id;
    case NameRole:
        return t.displayName();
    case SubtitleRole:
        return t.kind == Kind::Pwa ? QStringLiteral("App") : t.subtitle;
    case IconRole:
        return t.icon;
    case KindRole:
        return kindName(t.kind);
    case ShortcutRole:
        return index.row() < 9 ? QString::number(index.row() + 1) : QString();
    case SuggestedRole:
        return index.row() == 0;
    case IncognitoRole:
        return t.incognito;
    default:
        return {};
    }
}

QHash<int, QByteArray> PickerModel::roleNames() const
{
    return {
        {IdRole, "targetId"},
        {NameRole, "name"},
        {SubtitleRole, "subtitle"},
        {IconRole, "iconName"},
        {KindRole, "kind"},
        {ShortcutRole, "shortcut"},
        {SuggestedRole, "suggested"},
        {IncognitoRole, "incognito"},
    };
}

void PickerModel::reset(QList<Target> targets, const QString &filter)
{
    m_all = std::move(targets);
    m_filter = filter;
    applyFilter();
}

void PickerModel::setFilter(const QString &filter)
{
    if (m_filter == filter) {
        return;
    }
    m_filter = filter;
    applyFilter();
}

Target PickerModel::targetAt(int row) const
{
    if (row < 0 || row >= m_shown.size()) {
        return {};
    }
    return m_shown.at(row);
}

void PickerModel::applyFilter()
{
    beginResetModel();
    m_shown.clear();
    const QString needle = m_filter.trimmed();
    for (const auto &t : m_all) {
        if (needle.isEmpty()
            || t.displayName().contains(needle, Qt::CaseInsensitive)
            || t.subtitle.contains(needle, Qt::CaseInsensitive)
            || t.browserName.contains(needle, Qt::CaseInsensitive)
            || kindName(t.kind).contains(needle, Qt::CaseInsensitive)) {
            m_shown.append(t);
        }
    }
    endResetModel();
    Q_EMIT countChanged();
}

} // namespace Tern
