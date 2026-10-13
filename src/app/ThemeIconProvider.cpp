#include "ThemeIconProvider.h"

#include <QIcon>
#include <QPixmap>

namespace Lane
{

ThemeIconProvider::ThemeIconProvider()
    : QQuickImageProvider(QQuickImageProvider::Pixmap)
{
}

QPixmap ThemeIconProvider::requestPixmap(const QString &id, QSize *size, const QSize &requestedSize)
{
    // id can carry query params from the URL (e.g. "name?size=22");
    // QQuickImageProvider strips them, so id is just the icon name.
    const int px = requestedSize.width() > 0 ? requestedSize.width() : 22;
    const QString name = id;
    QIcon icon;
    if (!name.isEmpty()) {
        icon = QIcon::fromTheme(name);
    }
    if (icon.isNull()) {
        icon = QIcon::fromTheme(QStringLiteral("application-x-executable"));
    }
    if (icon.isNull()) {
        icon = QIcon::fromTheme(QStringLiteral("image-missing"));
    }
    QPixmap pixmap = icon.pixmap(px * 2, px * 2); // 2x for HiDPI
    if (size) {
        *size = pixmap.size();
    }
    return pixmap;
}

} // namespace Lane
