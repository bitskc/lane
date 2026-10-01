#include "PickerModel.h"

namespace Lane
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
    case ColorRole:
        return t.kind == Kind::Container && t.color.isValid() ? t.color.name() : QString();
    case ShortcutRole:
        // Must match Picker.qml's `maxRows` (8): that many number-key
        // shortcuts are bound, so only that many rows may claim one.
        return index.row() < 8 ? QString::number(index.row() + 1) : QString();
    case SuggestedRole:
        return t.suggested;
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
        {ColorRole, "colorName"},
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
    // Row order is exactly the order rankForPicker() produced -- the
    // user's targetOrder with unlisted targets appended in discovery
    // order. Filtering only removes rows; it never reorders or groups
    // them, so a section-free flat list always reflects what the user
    // arranged on the Settings page. The suggestion is carried on the
    // Target's `suggested` flag rather than pinned to row 0.
    const QString needle = m_filter.trimmed();
    m_shown.clear();
    m_suggestedIndex = -1;
    for (const auto &t : m_all) {
        if (needle.isEmpty()
            || t.displayName().contains(needle, Qt::CaseInsensitive)
            || t.subtitle.contains(needle, Qt::CaseInsensitive)
            || t.browserName.contains(needle, Qt::CaseInsensitive)
            || kindName(t.kind).contains(needle, Qt::CaseInsensitive)) {
            if (t.suggested && m_suggestedIndex < 0) {
                m_suggestedIndex = m_shown.size();
            }
            m_shown.append(t);
        }
    }

    endResetModel();
    Q_EMIT countChanged();
}

} // namespace Lane
