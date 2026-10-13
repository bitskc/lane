#pragma once

#include <QQuickImageProvider>

namespace Lane
{

// Provides freedesktop theme icons to QML via the laneicon image
// provider. Replaces Kirigami.Icon, whose IconImage backend is a
// private QtQuick.Controls.impl API. Renders at 2x to stay crisp on
// HiDPI; falls back to application-x-executable, then image-missing.
class ThemeIconProvider : public QQuickImageProvider
{
    Q_OBJECT

public:
    explicit ThemeIconProvider();

    QPixmap requestPixmap(const QString &id, QSize *size, const QSize &requestedSize) override;
};

} // namespace Lane
