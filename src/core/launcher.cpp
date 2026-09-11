#include "launcher.h"

#include <QProcess>

namespace Tern
{

QStringList expandArgs(const Target &target, const QString &url)
{
    QStringList out;
    bool placed = false;
    for (QString a : target.args) {
        if (a.contains(QLatin1String("$url")) || a.contains(QLatin1String("%url%")) || a.contains(QLatin1String("%u"))) {
            a.replace(QStringLiteral("$url"), url);
            a.replace(QStringLiteral("%url%"), url);
            a.replace(QStringLiteral("%u"), url);
            placed = true;
        }
        out << a;
    }
    if (!placed && target.kind != Kind::Action) {
        out << url;
    }
    return out;
}

bool launchTarget(const Target &target, const QString &url)
{
    if (target.kind == Kind::Action && target.id == QLatin1String("action:copy")) {
        return false;
    }
    if (target.exec.isEmpty()) {
        return false;
    }
    return QProcess::startDetached(target.exec, expandArgs(target, url));
}

} // namespace Tern
