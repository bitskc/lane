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
    // LaneIcon supplies physical-pixel sourceSize, including display DPR.
    const int px = requestedSize.width() > 0 ? requestedSize.width() : 22;
    const QString &name = id;
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
    QPixmap pixmap = icon.pixmap(px, px);
    if (size) {
        *size = pixmap.size();
    }
    return pixmap;
}

} // namespace Lane
