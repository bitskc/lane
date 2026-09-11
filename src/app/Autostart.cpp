#include "Autostart.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTextStream>

namespace Lane
{
namespace
{

QString autostartPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)
        + QStringLiteral("/autostart/app.lane.Lane.desktop");
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
    const QString exe = QCoreApplication::applicationFilePath();
    const QString exec = exe.isEmpty() ? QStringLiteral("lane --daemon")
                                        : exe + QStringLiteral(" --daemon");
    QTextStream s(&f);
    s << QStringLiteral("[Desktop Entry]\n")
      << QStringLiteral("Type=Application\n")
      << QStringLiteral("Name=Lane\n")
      << QStringLiteral("Comment=Open links in the right browser, profile, or app\n")
      << QStringLiteral("Exec=") << exec << QStringLiteral("\n")
      << QStringLiteral("Icon=app.lane.Lane\n")
      << QStringLiteral("Terminal=false\n")
      << QStringLiteral("Categories=Network;\n")
      << QStringLiteral("X-KDE-autostart-phase=1\n");
}

} // namespace Lane
