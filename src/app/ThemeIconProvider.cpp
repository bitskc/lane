#include "ThemeIconProvider.h"

#include "core/discovery.h"

#include <QFileIconProvider>
#include <QFileInfo>
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
    if (name.startsWith(QLatin1String("bundle:"))) {
        // macOS .app icons: QFileIconProvider on the bundle path, keyed
        // by bundle id so a rename does not break the QML source URL.
        const QString path = macBundlePathForId(name.mid(QStringLiteral("bundle:").size()));
        if (!path.isEmpty()) {
            icon = QFileIconProvider().icon(QFileInfo(path));
        }
    } else if (!name.isEmpty()) {
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
