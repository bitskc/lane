#include "launcher.h"

#include "urlutil.h"

#include <QFileInfo>
#include <QProcess>
#include <QSet>
#include <QRegularExpression>
#include <QStandardPaths>

namespace Lane
{
namespace
{

const QSet<QString> &blockedInterpreters()
{
    // Two families, both rejected purely by exec's basename (or, via
    // isBlockedInterpreterChain(), the basename of anything it symlinks
    // to):
    //
    // Shells and language runtimes: a target whose exec IS one of these
    // can be handed an arbitrary command through target.args.
    //
    // Re-exec wrappers: a target whose exec is one of these can be handed
    // a *different* program to run as its own args (env python -c ...,
    // xargs bash -c ..., sudo/pkexec anything, ssh host anything, and so
    // on), which reaches an interpreter without exec itself ever being
    // one. Blocking the wrapper outright closes that whole class without
    // having to parse its argv, which is not a fight this blocklist can
    // win in general (see isBlockedInterpreterChain() callers for the
    // residual risk this does not close).
    static const QSet<QString> k = {
        // shells and language runtimes
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
        // re-exec / process-wrapper binaries: each one's whole job is to
        // run something else, named in its own argv rather than in exec
        QStringLiteral("env"),
        QStringLiteral("xargs"),
        QStringLiteral("nohup"),
        QStringLiteral("setsid"),
        QStringLiteral("timeout"),
        QStringLiteral("stdbuf"),
        QStringLiteral("nice"),
        QStringLiteral("ionice"),
        QStringLiteral("watch"),
        QStringLiteral("sudo"),
        QStringLiteral("doas"),
        QStringLiteral("pkexec"),
        QStringLiteral("systemd-run"),
        QStringLiteral("flatpak-spawn"),
        QStringLiteral("ssh"),
        QStringLiteral("awk"),
        QStringLiteral("gawk"),
        QStringLiteral("find"),
        QStringLiteral("sed"),
    };
    return k;
}

// flatpak itself is not a blocked interpreter (legitimate browser desktop
// entries launch through `flatpak run`), but its argv can still name a
// blocked interpreter via `run --command=<prog>`, which is what actually
// executes inside the sandbox. A hand-edited target like
// exec=flatpak args="run --command=sh app.id" would otherwise sail past
// the exec-basename check and spawn a shell. Also require a dotted
// reverse-DNS app id in the args: `flatpak run` without one is not a
// browser launch at all.
bool flatpakRunArgsBlocked(const QStringList &args)
{
    static const QRegularExpression appIdPattern(QStringLiteral("^[A-Za-z0-9_-]+(\\.[A-Za-z0-9_-]+)+$"));
    bool hasAppId = false;
    for (qsizetype i = 0; i < args.size(); ++i) {
        const QString &a = args.at(i);
        if (a.startsWith(QLatin1String("--command="))) {
            if (isBlockedInterpreter(a.mid(QStringLiteral("--command=").size()))) {
                return true;
            }
        } else if (a == QLatin1String("--command") && i + 1 < args.size()) {
            if (isBlockedInterpreter(args.at(i + 1))) {
                return true;
            }
        } else if (!a.startsWith(QLatin1String("--")) && appIdPattern.match(a).hasMatch()) {
            hasAppId = true;
        }
    }
    return !hasAppId;
}

} // namespace

QStringList expandArgs(const Target &target, const QString &url)
{
    QStringList out;
    bool placed = false;
    const QString encodedUrl = QString::fromUtf8(QUrl::toPercentEncoding(url));
    for (QString a : target.args) {
        if (a.contains(QLatin1String("$urlEncoded"))) {
            a.replace(QStringLiteral("$urlEncoded"), encodedUrl);
            placed = true;
        }
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

bool isBlockedInterpreterChain(const QString &execPath)
{
    if (execPath.isEmpty()) {
        return true;
    }
    QString current = execPath;
    // Real executables are never nested this many symlinks deep; the cap
    // exists only to guarantee termination against a pathological or
    // looping chain, not because any legitimate install needs it.
    static constexpr int kMaxHops = 40;
    for (int hop = 0; hop < kMaxHops; ++hop) {
        if (isBlockedInterpreter(current)) {
            return true;
        }
        const QFileInfo info(current);
        if (!info.exists()) {
            // Fail closed: a hop that vanished mid-check (broken link,
            // deleted-out-from-under-us race) cannot be vouched for.
            return true;
        }
        if (!info.isSymLink()) {
            return false;
        }
        const QString target = info.symLinkTarget();
        if (target.isEmpty() || target == current) {
            return true;
        }
        current = target;
    }
    return true;
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
        // Deliberately not canonicalFilePath() here: collapsing straight
        // to the final symlink target before the blocklist check would
        // launder a literally blocked name (python3 -> python3.14 on this
        // machine, for instance) past isBlockedInterpreter() instead of
        // being caught by it. isBlockedInterpreterChain() below walks the
        // symlink chain itself, checking every hop's basename starting
        // from this one, which is what actually needs to happen.
        return info.isExecutable() ? trimmed : QString();
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
    if (isBlockedInterpreterChain(resolved)) {
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

bool launchTarget(const Target &target, const QString &url, const QString &activationToken)
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
    // Always resolve, even for an absolute exec: this is what confirms
    // the file actually exists and is executable rather than trusting an
    // unresolved string straight from a Target that may have been built
    // from a hand-edited config.json, and it is a precondition for the
    // symlink-chain check right below.
    const QString exe = resolveExecutable(target.exec);
    if (exe.isEmpty()) {
        return false;
    }
    // isBlockedInterpreterChain(), not the plain basename check: exe can
    // itself be a symlink (resolveExecutable() deliberately does not
    // canonicalize) pointing at a blocked interpreter under an unrelated
    // name, and the chain walk is what catches that.
    if (isBlockedInterpreterChain(exe)) {
        return false;
    }
    // flatpak passes the exec check legitimately, so its argv needs its
    // own inspection: `run --command=<prog>` names the program that
    // actually executes, and it must not be a blocked interpreter, and a
    // `flatpak` invocation with no dotted app id is not a browser launch.
    if (QFileInfo(exe).fileName() == QLatin1String("flatpak")
        && flatpakRunArgsBlocked(expandArgs(target, open))) {
        return false;
    }
    // QProcess::startDetached(program, arguments) (the static, argument-only
    // overload used below) takes no QProcessEnvironment and always forks
    // from Lane's own live environment, unlike setProcessEnvironment() on a
    // QProcess instance, which Qt documents as not always applying to a
    // detached start. Setting XDG_ACTIVATION_TOKEN here, immediately around
    // the spawn, and restoring whatever was there before right after, is
    // therefore both correct for this call shape and as narrow a window as
    // it allows. An empty activationToken leaves the environment untouched
    // and this call behaves exactly as it always has.
    //
    // This mutates the whole process's environment, not just this call's
    // view of it; it is safe only because Lane's Qt event loop is
    // single-threaded, so no other code can run (and read a stale or
    // half-restored value of XDG_ACTIVATION_TOKEN) between the set here
    // and the restore below. A second launchTarget() call could only
    // interleave via re-entrancy on this same thread, and none does.
    const bool hadToken = !activationToken.isEmpty();
    const bool hadPrevious = qEnvironmentVariableIsSet("XDG_ACTIVATION_TOKEN");
    const QByteArray previous = hadPrevious ? qgetenv("XDG_ACTIVATION_TOKEN") : QByteArray();
    if (hadToken) {
        qputenv("XDG_ACTIVATION_TOKEN", activationToken.toUtf8());
    }
    const bool started = QProcess::startDetached(exe, expandArgs(target, open));
    if (hadToken) {
        if (hadPrevious) {
            qputenv("XDG_ACTIVATION_TOKEN", previous);
        } else {
            qunsetenv("XDG_ACTIVATION_TOKEN");
        }
    }
    return started;
}

} // namespace Lane
