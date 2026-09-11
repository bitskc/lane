#include "PickerModel.h"

#include <QHash>

namespace Lane
{

static QString sectionFor(const Target &t);

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
    case SectionRole:
        return sectionFor(t);
    case ColorRole:
        return t.kind == Kind::Container && t.color.isValid() ? t.color.name() : QString();
    case ShortcutRole:
        // Must match Picker.qml's `maxRows` (8): that many number-key
        // shortcuts are bound, so only that many rows may claim one.
        return index.row() < 8 ? QString::number(index.row() + 1) : QString();
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
        {SectionRole, "section"},
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

static QString sectionFor(const Target &t)
{
    switch (t.kind) {
    case Kind::Container:
        return QStringLiteral("Containers");
    case Kind::Pwa:
        return QStringLiteral("Web apps");
    case Kind::Action:
        return QStringLiteral("Actions");
    case Kind::Custom:
        return QStringLiteral("Apps");
    case Kind::BrowserProfile:
        return QStringLiteral("Browsers");
    }
    return QStringLiteral("Browsers");
}

void PickerModel::applyFilter()
{
    beginResetModel();
    m_shown.clear();
    const QString needle = m_filter.trimmed();
    QList<Target> matched;
    for (const auto &t : m_all) {
        if (needle.isEmpty()
            || t.displayName().contains(needle, Qt::CaseInsensitive)
            || t.subtitle.contains(needle, Qt::CaseInsensitive)
            || t.browserName.contains(needle, Qt::CaseInsensitive)
            || kindName(t.kind).contains(needle, Qt::CaseInsensitive)) {
            matched.append(t);
        }
    }

    // rankForPicker() already decided priority order (current-site PWA and
    // remembered destination lead); group same-kind rows together for the
    // display only, via a stable partition keyed on each section's first
    // occurrence, so the ranking itself is never touched but a 45-target
    // list is still scannable in clusters instead of interleaved.
    QHash<QString, QList<Target>> buckets;
    QStringList sectionOrder;
    for (const auto &t : matched) {
        const QString key = sectionFor(t);
        if (!buckets.contains(key)) {
            sectionOrder << key;
        }
        buckets[key].append(t);
    }
    for (const auto &key : sectionOrder) {
        m_shown += buckets.value(key);
    }
    m_sectionCount = sectionOrder.size();

    endResetModel();
    Q_EMIT countChanged();
}

} // namespace Lane
