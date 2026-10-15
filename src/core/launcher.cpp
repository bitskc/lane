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

// macOS-only additions to the exec blocklist above, applied by
// isBlockedInterpreterChainMac(). `open` and `osascript` are the Launch-
// Services and AppleScript back doors: a user-authored custom: target that
// names either could launch or drive any installed app, which is exactly
// what the interpreter blocklist exists to prevent. The rest are macOS
// scripting runtimes and wrappers with no Linux counterpart worth banning
// (`open` is a plain text/URL viewer on Linux desktops that have one, so
// the Linux list deliberately does not add it). osascript sits in the base
// list already; it is repeated here only so this set reads as the spec's
// complete macOS addition list.
const QSet<QString> &macBlockedExecNames()
{
    static const QSet<QString> k = {
        QStringLiteral("open"),
        QStringLiteral("osascript"),
        QStringLiteral("swift"),
        QStringLiteral("script"),
        QStringLiteral("tclsh"),
        QStringLiteral("wish"),
        QStringLiteral("shortcuts"),
        QStringLiteral("lldb"),
        QStringLiteral("automator"),
    };
    return k;
}

// Shared walk behind isBlockedInterpreterChain() and its macOS superset.
// With macRules on, every hop's basename is additionally matched against
// macBlockedExecNames() -- so a custom command symlinked to /usr/bin/open
// is caught at the final hop's basename too -- and any hop (including the
// final resolved path) that is a directory or ends in .app fails closed.
// A directory is never a launchable program, and a .app path means the
// command names a bundle directory or a file inside one, which must go
// through the fixed /usr/bin/open route instead of exec'ing directly.
bool blockedChain(const QString &execPath, bool macRules)
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
        if (isBlockedInterpreter(current)
            || (macRules && macBlockedExecNames().contains(QFileInfo(current).fileName().toLower()))) {
            return true;
        }
        const QFileInfo info(current);
        if (!info.exists()) {
            // Fail closed: a hop that vanished mid-check (broken link,
            // deleted-out-from-under-us race) cannot be vouched for.
            return true;
        }
        if (macRules
            && (info.isDir() || info.fileName().endsWith(QLatin1String(".app"), Qt::CaseInsensitive))) {
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

// The chain check this build actually runs: the macOS rule set on macOS,
// the unchanged Linux set everywhere else.
bool osChainBlocked(const QString &execPath)
{
#ifdef Q_OS_MACOS
    return isBlockedInterpreterChainMac(execPath);
#else
    return isBlockedInterpreterChain(execPath);
#endif
}

// flatpak itself is not a blocked interpreter (legitimate browser desktop
// entries launch through `flatpak run`), but its argv can still name a
// blocked interpreter via `run --command=<prog>`, which is what actually
// executes inside the sandbox. A hand-edited target like
// exec=flatpak args="run --command=sh app.id" would otherwise sail past
// the exec-basename check and spawn a shell.
//
// flatpak's extract_command() takes the FIRST non-dash token as the
// subcommand and keeps every option token — including ones before it —
// for that subcommand's GOption parse. So `flatpak --command=sh run
// app.id` really does run sh, and no pre-command option consumes a
// separate value token (`flatpak --installation x list` errors "x is not
// a flatpak command"). Two consequences: dangerous run-options must be
// scanned position-independently, and the pre-subcommand skip is a plain
// dash-prefix skip. The first non-option token must be exactly `run`
// (other subcommands like `enter`, `build`, or `debug` are not browser
// launches), and a dotted reverse-DNS app id must be present.
bool flatpakRunArgsBlocked(const QStringList &args)
{
    // Scan flatpak run-options that can name an alternate command:
    // --command replaces the sandboxed entry point outright, so
    // `flatpak run --command=sh app.id` really does run sh instead of the
    // app. Only reject when the option's value names a blocked
    // interpreter, not for every occurrence of --command.
    //
    // Non-goal: --env, --filesystem, --socket, --device, and the rest of
    // flatpak's sandbox-widening options are deliberately NOT policed
    // here. Matching their option names against isBlockedInterpreter()
    // only checks the value's basename, which produces false positives:
    // `--env=SHELL=/bin/bash` reads as basename "bash" and would block a
    // legitimate launch that never runs a shell. The gate's declared
    // scope is option values that replace the program flatpak execs;
    // widening that scope is a product decision, not a cleanup.
    static const QSet<QString> dangerousOpts = {
        QStringLiteral("--command"),
    };
    for (qsizetype i = 0; i < args.size(); ++i) {
        const QString &a = args.at(i);
        const qsizetype eq = a.indexOf(QLatin1Char('='));
        const QString opt = eq < 0 ? a : a.left(eq);
        if (!dangerousOpts.contains(opt)) {
            continue;
        }
        QString value;
        if (eq >= 0) {
            value = a.mid(eq + 1);
        } else if (i + 1 < args.size()) {
            value = args.at(i + 1);
        }
        if (isBlockedInterpreter(value)) {
            return true;
        }
    }

    qsizetype i = 0;
    for (; i < args.size(); ++i) {
        const QString &a = args.at(i);
        if (a == QLatin1String("--")) {
            ++i;
            break;
        }
        if (!a.startsWith(QLatin1Char('-'))) {
            break;
        }
    }
    if (i >= args.size() || args.at(i) != QLatin1String("run")) {
        return true;
    }
    static const QRegularExpression appIdPattern(QStringLiteral("^[A-Za-z0-9_-]+(\\.[A-Za-z0-9_-]+)+$"));
    bool hasAppId = false;
    for (++i; i < args.size(); ++i) {
        const QString &a = args.at(i);
        if (!a.startsWith(QLatin1String("--")) && appIdPattern.match(a).hasMatch()) {
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
    return blockedChain(execPath, false);
}

bool isBlockedInterpreterChainMac(const QString &execPath)
{
    return blockedChain(execPath, true);
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
    if (osChainBlocked(resolved)) {
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

std::optional<QStringList> macFixedOpenArgs(const Target &target, const QString &sanitizedUrl)
{
    if (target.kind == Kind::Action && target.id == QLatin1String("action:email")) {
        if (sanitizedUrl.isEmpty()) {
            // Owned but refused: `open` with no argument opens Finder.
            return QStringList();
        }
        // The whole URL is percent-encoded into a single mailto body so a
        // crafted query string cannot inject to, cc or subject fields.
        return QStringList{QStringLiteral("mailto:?body=")
                           + QString::fromUtf8(QUrl::toPercentEncoding(sanitizedUrl))};
    }
    if (target.kind == Kind::BrowserProfile && target.bundleId == QLatin1String("com.apple.Safari")) {
        if (sanitizedUrl.isEmpty()) {
            return QStringList();
        }
        // Safari is reached through LaunchServices, not by exec'ing the
        // bundle binary, and takes no profile or private arguments.
        return QStringList{QStringLiteral("-b"), target.bundleId, sanitizedUrl};
    }
    return std::nullopt;
}

bool launchTarget(const Target &target, const QString &url, const QString &activationToken)
{
    if (target.kind == Kind::Action && target.id == QLatin1String("action:copy")) {
        return false;
    }
    const QString open = sanitizedOpenUrl(url);
    if (open.isEmpty()) {
        return false;
    }
#ifdef Q_OS_MACOS
    // Fixed /usr/bin/open routes (Safari, action:email) are built from the
    // target's kind and bundle id and deliberately never look at
    // target.exec, so they run before the exec resolution and chain checks
    // -- which would otherwise refuse `/usr/bin/open` itself, since `open`
    // is on the macOS exec blocklist. The path is fixed, so no
    // resolveExecutable() or chain check applies to it.
    if (const auto openArgs = macFixedOpenArgs(target, open)) {
        if (openArgs->isEmpty()) {
            return false;
        }
        return QProcess::startDetached(QStringLiteral("/usr/bin/open"), *openArgs);
    }
#endif
    if (target.exec.isEmpty()) {
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
    // osChainBlocked(), not the plain basename check: exe can itself be a
    // symlink (resolveExecutable() deliberately does not canonicalize)
    // pointing at a blocked interpreter under an unrelated name, and the
    // chain walk is what catches that.
    if (osChainBlocked(exe)) {
        return false;
    }
    // flatpak passes the exec check legitimately, so its argv needs its
    // own inspection: `run --command=<prog>` names the program that
    // actually executes, and it must not be a blocked interpreter, and a
    // `flatpak` invocation with no dotted app id is not a browser launch.
    // The canonical path is checked too: resolveExecutable() deliberately
    // does not canonicalize, so a symlink named "browser" pointing at
    // /usr/bin/flatpak would otherwise skip this gate entirely.
    const QString exeName = QFileInfo(exe).fileName();
    const QString canonicalName = QFileInfo(QFileInfo(exe).canonicalFilePath()).fileName();
    if ((exeName == QLatin1String("flatpak") || canonicalName == QLatin1String("flatpak"))
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
