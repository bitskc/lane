#include "Autostart.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTextStream>

namespace Tern
{
namespace
{

QString autostartPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)
        + QStringLiteral("/autostart/app.tern.Tern.desktop");
}

} // namespace

bool autostartEnabled()
{
    return QFile::exists(autostartPath());
}

void setAutostart(bool enabled)
{
    const QString path = autostartPath();
    if (!enabled) {
        QFile::remove(path);
        return;
    }
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return;
    }
    QTextStream s(&f);
    s << QStringLiteral("[Desktop Entry]\n")
      << QStringLiteral("Type=Application\n")
      << QStringLiteral("Name=Tern\n")
      << QStringLiteral("Comment=Open links in the right browser, profile, or app\n")
      << QStringLiteral("Exec=tern --daemon\n")
      << QStringLiteral("Icon=app.tern.Tern\n")
      << QStringLiteral("Terminal=false\n")
      << QStringLiteral("Categories=Network;\n")
      << QStringLiteral("X-KDE-autostart-phase=1\n");
}

} // namespace Tern
