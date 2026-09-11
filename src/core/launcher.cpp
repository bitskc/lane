#include "launcher.h"

#include "urlutil.h"

#include <QFileInfo>
#include <QProcess>
#include <QSet>
#include <QStandardPaths>

namespace Tern
{
namespace
{

const QSet<QString> &blockedInterpreters()
{
    static const QSet<QString> k = {
        QStringLiteral("sh"),
        QStringLiteral("bash"),
        QStringLiteral("zsh"),
        QStringLiteral("fish"),
        QStringLiteral("dash"),
        QStringLiteral("csh"),
        QStringLiteral("tcsh"),
        QStringLiteral("ksh"),
        QStringLiteral("busybox"),
        QStringLiteral("python"),
        QStringLiteral("python3"),
        QStringLiteral("perl"),
        QStringLiteral("ruby"),
        QStringLiteral("node"),
        QStringLiteral("nodejs"),
        QStringLiteral("lua"),
        QStringLiteral("php"),
        QStringLiteral("osascript"),
        QStringLiteral("cmd.exe"),
        QStringLiteral("powershell"),
        QStringLiteral("pwsh"),
    };
    return k;
}

} // namespace

QStringList expandArgs(const Target &target, const QString &url)
{
    QStringList out;
    bool placed = false;
    for (QString a : target.args) {
        if (a.contains(QLatin1String("$url")) || a.contains(QLatin1String("%url%"))
            || a.contains(QLatin1String("%u"))) {
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

bool isBlockedInterpreter(const QString &exec)
{
    return blockedInterpreters().contains(QFileInfo(exec).fileName().toLower());
}

QString resolveExecutable(const QString &exec)
{
    const QString trimmed = exec.trimmed();
    if (trimmed.isEmpty()) {
        return {};
    }
    if (trimmed.contains(QLatin1Char('\0')) || trimmed.contains(QLatin1Char('\n'))) {
        return {};
    }
    const QFileInfo info(trimmed);
    if (info.isAbsolute()) {
        return info.isExecutable() ? info.canonicalFilePath() : QString();
    }
    return QStandardPaths::findExecutable(trimmed);
}

bool parseCustomCommand(const QString &command, Target *out, QString *error)
{
    const QStringList parts = QProcess::splitCommand(command.trimmed());
    if (parts.isEmpty()) {
        if (error) {
            *error = QStringLiteral("Command is empty");
        }
        return false;
    }
    const QString resolved = resolveExecutable(parts.first());
    if (resolved.isEmpty()) {
        if (error) {
            *error = QStringLiteral("Executable not found");
        }
        return false;
    }
    if (isBlockedInterpreter(resolved)) {
        if (error) {
            *error = QStringLiteral("Shells and language runtimes are not allowed as handlers");
        }
        return false;
    }
    if (!out) {
        return true;
    }
    out->kind = Kind::Custom;
    out->engine = Engine::Generic;
    out->exec = resolved;
    out->args = parts.mid(1);
    bool placed = false;
    for (const auto &a : out->args) {
        if (a.contains(QLatin1String("$url")) || a.contains(QLatin1String("%url%"))
            || a.contains(QLatin1String("%u"))) {
            placed = true;
            break;
        }
    }
    if (!placed) {
        out->args << QStringLiteral("$url");
    }
    return true;
}

bool launchTarget(const Target &target, const QString &url)
{
    if (target.kind == Kind::Action && target.id == QLatin1String("action:copy")) {
        return false;
    }
    if (target.exec.isEmpty()) {
        return false;
    }
    const QString open = sanitizedOpenUrl(url);
    if (open.isEmpty()) {
        return false;
    }
    const QString exe = QFileInfo(target.exec).isAbsolute() ? target.exec : resolveExecutable(target.exec);
    if (exe.isEmpty()) {
        return false;
    }
    return QProcess::startDetached(exe, expandArgs(target, open));
}

} // namespace Tern
