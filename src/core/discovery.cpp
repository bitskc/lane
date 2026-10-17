#include "discovery.h"

#include "urlutil.h"

#include <algorithm>
#include <initializer_list>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QSettings>
#include <QHash>
#include <QStandardPaths>
#include <QSet>
#include <QDebug>
#include <QRegularExpression>
#include <QVariant>
#include <QXmlStreamReader>


namespace Lane
{
namespace
{

struct DesktopApp {
    QString id;
    QString name;
    QString execLine;
    QString icon;
    QString wmClass;
    QStringList categories;
    QString mime;
    // Set only for .app bundles found by the macOS scanner; empty for
    // desktop-file entries. Copied onto every Target the walkers produce
    // for this app.
    QString bundleId;
    QString bundlePath;
    bool noDisplay = false;
};

// Splits a desktop Exec= line into tokens, honoring double quotes the way
// firstToken() already did for the program token: a quoted span is one
// token with the quotes removed, so `--command="zen browser"` survives as
// a single `--command=zen browser` argument instead of being split apart
// and corrupting the rebuilt argv.
QStringList splitExecTokens(const QString &execLine)
{
    QStringList out;
    QString cur;
    bool inQuote = false;
    bool hasToken = false;
    for (const QChar c : execLine) {
        if (c == QLatin1Char('"')) {
            inQuote = !inQuote;
            hasToken = true;
            continue;
        }
        if (!inQuote && c == QLatin1Char(' ')) {
            if (hasToken) {
                out << cur;
                cur.clear();
                hasToken = false;
            }
            continue;
        }
        cur += c;
        hasToken = true;
    }
    if (hasToken) {
        out << cur;
    }
    return out;
}

// Splits a desktop Exec= line into its program (matching firstToken()'s
// quoting rules) and the remaining tokens with freedesktop field codes
// (%f %F %u %U %d %D %n %N %i %c %k %v %m) and Flatpak's "@@u ... @@"
// file-forwarding markers stripped. For a Flatpak entry ("flatpak run
// --branch=stable --arch=x86_64 --command=zen app.zen_browser.zen %u")
// it is the "run ... <app-id>" tokens that must stay in front of Lane's
// own --profile/--new-tab args, or the launched process becomes
// "flatpak --profile ..." instead of the browser. For a native entry the
// prefix holds the browser's own flags, which callers must NOT prepend
// (see flatpakPrefixArgs).
//
// A leading `env VAR=value ...` wrapper is unwrapped: `env` and each
// assignment token are skipped and the first real program token becomes
// the program. Without this, `Exec=env MOZ_X11=1 firefox %u` yields
// exec=env, which the launcher's interpreter blocklist rejects, so the
// entry would show up in settings but fail on every launch. When nothing
// remains after the wrapper, program is left empty and the caller drops
// the entry.
struct ExecPrefix {
    QString program;
    QStringList args;
    // Set when an `env` wrapper's option scan hit an option this parser
    // could not resolve (unknown long option, an ambiguous abbreviation
    // of two or more known ones, or -S/--split-string) and gave up rather
    // than guess. Distinguishes that case from a syntactically fine
    // wrapper that simply never names a program (e.g. a bare `env`),
    // which also leaves `program` empty but is not a parse failure.
    bool unparseable = false;
};

bool isExecFieldCode(const QString &token)
{
    // Flatpak's file-forwarding wrapper emits an unpaired "@@u" opening
    // marker (not bare "@@") alongside the field code it wraps and a
    // trailing bare "@@" closer, e.g. "--file-forwarding app-id @@u %u @@".
    // Matching only exact "@@" left "@@u" in the stripped output, where it
    // corrupts flatpak run's own argv parsing (flatpak reads "@@u ... @@"
    // as a forwarding span). Any token starting with "@@" is a marker.
    static const QRegularExpression fieldCode(QStringLiteral("^%[fFuUdDnNickvm]$"));
    return token.startsWith(QLatin1String("@@")) || fieldCode.match(token).hasMatch();
}

// Consumes one `env` short-option token (e.g. "-i", or a bundled cluster
// like "-iu" the way getopt allows i and u to combine). Returns false,
// setting `unparseable`, when the token names an option this parser
// cannot resolve to a fixed valueless/value-taking shape (-S/--split-string,
// or any character outside env's known short options): the caller must
// then fail closed rather than risk treating the following token as the
// program when it is really that option's value.
bool consumeEnvShortOpts(const QString &tok, QStringList &tokens, bool &unparseable)
{
    for (qsizetype i = 1; i < tok.size(); ++i) {
        const QChar c = tok.at(i);
        if (c == QLatin1Char('i') || c == QLatin1Char('0') || c == QLatin1Char('v')) {
            continue; // -i/--ignore-environment, -0/--null, -v/--debug: valueless
        }
        if (c == QLatin1Char('S')) {
            // -S/--split-string takes a single shell-syntax string that
            // this tokenizer does not interpret; parsing the rest of argv
            // as plain Exec tokens could pick one of its words as the
            // program. Fail closed instead of guessing.
            unparseable = true;
            return false;
        }
        if (c == QLatin1Char('u') || c == QLatin1Char('C') || c == QLatin1Char('a')) {
            // -u NAME / -C DIR / -a ARGV0: the value is whatever is left
            // of this token ("-uNAME"), or the next token if nothing is
            // left ("-u NAME", or the last char of a bundle like "-iu
            // NAME"). Either way it stops this token's option scan.
            const QString rest = tok.mid(i + 1);
            if (rest.isEmpty() && !tokens.isEmpty()) {
                tokens.takeFirst();
            }
            return true;
        }
        // Any other short option is not one env supports; a real `env`
        // invocation with it would fail before ever reaching a program,
        // so the rest of argv cannot be trusted to start with one.
        unparseable = true;
        return false;
    }
    return true;
}

ExecPrefix execPrefix(const QString &execLine)
{
    ExecPrefix out;
    QStringList tokens = splitExecTokens(execLine);
    if (tokens.isEmpty()) {
        return out;
    }
    out.program = tokens.takeFirst();
    if (QFileInfo(out.program).fileName() == QLatin1String("env")) {
        // `env [opts] VAR=value ... prog`: skip env's own option flags
        // first, then the assignments, and keep the first real program
        // token. Without the option skip, `env -i firefox` unwraps to
        // program=-i, a dead target. A bare `env` (or only
        // options/assignments) leaves program empty and the entry is
        // dropped.
        //
        // Options are recognized from an explicit allowlist rather than
        // "anything starting with '-'": an option this allowlist does not
        // know, or one whose value syntax it cannot parse
        // (-S/--split-string takes a whole shell command line as one
        // string), means the rest of argv is not reliably "value* then
        // program" any more, so the whole entry is dropped instead of
        // guessing and possibly unwrapping to one of env's own flags or
        // option values as a fake "program".
        out.program.clear();
        static const QRegularExpression assignment(QStringLiteral("^[A-Za-z_][A-Za-z0-9_]*=.*$"));
        // Long options that consume the NEXT token as their value when
        // not given as --opt=value.
        static const QSet<QString> envLongOptWithValue = {
            QStringLiteral("--unset"), QStringLiteral("--chdir"), QStringLiteral("--argv0"),
        };
        // Long options that take no value at all; a real `env` errors if
        // one of these is given as --opt=value, so this parser matches
        // that and fails closed rather than silently accepting it.
        // (GNU env's --verbose does not exist; the valueless verbose/debug
        // flag is -v/--debug, already covered by --debug and by 'v' in
        // consumeEnvShortOpts.)
        static const QSet<QString> envLongOptNoValue = {
            QStringLiteral("--ignore-environment"), QStringLiteral("--null"),
            QStringLiteral("--debug"),
            // no_argument in GNU env: it errors on --list-signal-handling=x,
            // so it lives here, not in the optional-value set below.
            QStringLiteral("--list-signal-handling"),
        };
        // Long options that take an OPTIONAL value, only via --opt=value
        // (never the next token). Lane ignores the value either way, so
        // an attached "=..." is simply tolerated rather than parsed.
        static const QSet<QString> envLongOptOptionalValue = {
            QStringLiteral("--block-signal"), QStringLiteral("--default-signal"),
            QStringLiteral("--ignore-signal"),
        };
        bool optionsDone = false;
        bool unparseable = false;
        while (!unparseable && !tokens.isEmpty()) {
            const QString tok = tokens.takeFirst();
            if (!optionsDone) {
                if (tok == QLatin1String("--") || tok == QLatin1String("-")) {
                    // GNU env treats a bare "-" exactly like "--": both end
                    // option scanning. GNU env also treats a bare "-" as
                    // shorthand for -i (ignore-environment), but Lane never
                    // re-execs env (it only inspects Exec= to find the
                    // wrapped program), so that -i effect is vacuous here;
                    // only the terminator effect matters.
                    optionsDone = true;
                    continue;
                }
                if (tok.startsWith(QLatin1String("--"))) {
                    const qsizetype eq = tok.indexOf(QLatin1Char('='));
                    QString name = eq < 0 ? tok : tok.left(eq);
                    const bool knownExactly = name == QLatin1String("--split-string")
                        || envLongOptWithValue.contains(name) || envLongOptOptionalValue.contains(name)
                        || envLongOptNoValue.contains(name);
                    if (!knownExactly) {
                        // Not an exact match against any known long
                        // option: try resolving it as an unambiguous
                        // prefix of exactly one, the way getopt_long does
                        // ("--ignore-env" -> "--ignore-environment",
                        // "--chd" -> "--chdir"). A prefix of two or more
                        // known options (e.g. "--i", matching both
                        // --ignore-environment and --ignore-signal) is
                        // itself an ambiguity error in real getopt_long,
                        // so that case is left unresolved here and falls
                        // through to the same fail-closed branch below as
                        // a genuinely unknown option.
                        static const QStringList allLongOpts = [] {
                            QStringList l;
                            l << envLongOptWithValue.values() << envLongOptOptionalValue.values()
                              << envLongOptNoValue.values() << QStringLiteral("--split-string");
                            return l;
                        }();
                        QString onlyMatch;
                        int matchCount = 0;
                        for (const auto &known : allLongOpts) {
                            if (known.startsWith(name)) {
                                onlyMatch = known;
                                if (++matchCount > 1) {
                                    break;
                                }
                            }
                        }
                        if (matchCount == 1) {
                            name = onlyMatch;
                        }
                    }
                    if (name == QLatin1String("--split-string")) {
                        unparseable = true;
                        break;
                    }
                    if (envLongOptWithValue.contains(name)) {
                        if (eq < 0 && !tokens.isEmpty()) {
                            tokens.takeFirst();
                        }
                        continue;
                    }
                    if (envLongOptOptionalValue.contains(name)) {
                        continue;
                    }
                    if (envLongOptNoValue.contains(name)) {
                        if (eq >= 0) {
                            // e.g. "--debug=x": this option takes no value
                            // at all, not even optionally; real env errors
                            // on it, so fail closed instead of silently
                            // accepting and dropping the "=x".
                            unparseable = true;
                            break;
                        }
                        continue;
                    }
                    // Unrecognized long option, or an ambiguous prefix of
                    // two or more known ones: fail closed.
                    unparseable = true;
                    break;
                }
                if (tok.startsWith(QLatin1Char('-'))) {
                    if (!consumeEnvShortOpts(tok, tokens, unparseable)) {
                        break;
                    }
                    continue;
                }
                optionsDone = true;
            }
            if (assignment.match(tok).hasMatch()) {
                continue;
            }
            if (isExecFieldCode(tok)) {
                // A field code (e.g. bare "%u") can never be the program;
                // it is Lane/the desktop spec's own placeholder. Skip it
                // rather than accepting it as a fake "program" (an Exec=
                // that unwraps to nothing but options and field codes
                // leaves program empty and the entry is dropped).
                continue;
            }
            out.program = tok;
            break;
        }
        out.unparseable = unparseable;
    }
    for (const auto &t : tokens) {
        if (!isExecFieldCode(t)) {
            out.args << t;
        }
    }
    return out;
}

QString firstToken(const QString &execLine)
{
    return execPrefix(execLine).program;
}

// Pulls a Flatpak application id (e.g. "app.zen_browser.zen") out of the
// already-field-code-stripped remainder of a `flatpak run --branch=...
// --arch=... --command=... <app-id>` Exec= line. Flatpak app ids are
// reverse-DNS identifiers (at least two dots); that shape is enough to
// tell the id apart from "run" and the surrounding "--flag" tokens
// without special-casing every flatpak(1) option.
QString flatpakAppId(const QStringList &prefixArgs)
{
    static const QRegularExpression appIdPattern(QStringLiteral("^[A-Za-z0-9_-]+(\\.[A-Za-z0-9_-]+)+$"));
    for (const auto &tok : prefixArgs) {
        if (!tok.startsWith(QLatin1String("--")) && appIdPattern.match(tok).hasMatch()) {
            return tok;
        }
    }
    return QString();
}

bool skipDesktopId(const QString &id)
{
    const QString lower = id.toLower();
    return lower.startsWith(QLatin1String("app.lane"))
        || lower.startsWith(QLatin1String("ffpwa-"))
        || lower.startsWith(QLatin1String("userapp-"))
        || lower.contains(QLatin1String("firefoxpwa"))
        || lower.contains(QLatin1String("browser_integration"))
        || lower.contains(QLatin1String("browsertamer"))
        || lower == QLatin1String("bt")
        || lower.contains(QLatin1String("junction"));
}

// The Exec= prefix tokens only belong in front of Lane's own args when
// the entry actually launches through `flatpak run` (they are flatpak's
// "run ... <app-id>" arguments). For a native entry like
// "Exec=/usr/bin/firefox --new-window %u" they are the browser's own
// flags, and prepending them would put "--new-window" in front of
// "--profile <dir>", where the browser reads <dir> as the URL to open.
QStringList flatpakPrefixArgs(const ExecPrefix &prefix)
{
    if (QFileInfo(prefix.program).fileName() == QLatin1String("flatpak")) {
        return prefix.args;
    }
    return {};
}

QList<DesktopApp> scanDesktopFiles(const QStringList &dirs)
{
    QList<DesktopApp> apps;
    QSet<QString> seen;
    for (const QString &dir : dirs) {
        const QDir d(dir);
        if (!d.exists()) {
            continue;
        }
        const auto files = d.entryList({QStringLiteral("*.desktop")}, QDir::Files);
        for (const QString &file : files) {
            const QString id = QFileInfo(file).completeBaseName();
            if (seen.contains(id) || skipDesktopId(id)) {
                continue;
            }
            QFile f(d.filePath(file));
            if (!f.open(QIODevice::ReadOnly)) {
                continue;
            }
            DesktopApp app;
            app.id = id;
            bool inEntry = false;
            while (!f.atEnd()) {
                const QString line = QString::fromUtf8(f.readLine()).trimmed();
                if (line == QLatin1String("[Desktop Entry]")) {
                    inEntry = true;
                    continue;
                }
                if (line.startsWith(QLatin1Char('['))) {
                    inEntry = false;
                    continue;
                }
                if (!inEntry || !line.contains(QLatin1Char('='))) {
                    continue;
                }
                const QString key = line.section(QLatin1Char('='), 0, 0);
                const QString value = line.section(QLatin1Char('='), 1);
                if (key == QLatin1String("Name")) {
                    app.name = value;
                } else if (key == QLatin1String("Exec")) {
                    app.execLine = value;
                } else if (key == QLatin1String("Icon")) {
                    app.icon = value;
                } else if (key == QLatin1String("StartupWMClass")) {
                    app.wmClass = value;
                } else if (key == QLatin1String("Categories")) {
                    app.categories = value.split(QLatin1Char(';'), Qt::SkipEmptyParts);
                } else if (key == QLatin1String("MimeType")) {
                    app.mime = value;
                } else if (key == QLatin1String("NoDisplay") || key == QLatin1String("Hidden")) {
                    app.noDisplay = value.compare(QLatin1String("true"), Qt::CaseInsensitive) == 0;
                }
            }
            if (app.noDisplay || app.execLine.isEmpty()) {
                continue;
            }
            const bool browser = app.categories.contains(QLatin1String("WebBrowser"))
                || app.mime.contains(QLatin1String("x-scheme-handler/http"));
            if (!browser) {
                continue;
            }
            // An Exec= line that unwraps to no real program (e.g. a bare
            // `env VAR=...` with nothing after it) can never launch;
            // drop the entry rather than listing a dead target. When the
            // cause is specifically an env option this parser could not
            // resolve (as opposed to a wrapper that legitimately names no
            // program), log which desktop entry was dropped and why: this
            // used to fail silently, which turned a discovery false
            // negative into unrecoverable data loss once
            // Controller::reload() pruned the now-"dead" remembered hosts
            // pointing at it.
            const ExecPrefix prefix = execPrefix(app.execLine);
            if (prefix.program.isEmpty()) {
                if (prefix.unparseable) {
                    qDebug() << "Lane: dropping desktop entry" << id
                             << "- its Exec= env wrapper uses an option this parser does not recognize:"
                             << app.execLine;
                }
                continue;
            }
            seen.insert(id);
            apps.append(app);
        }
    }
    return apps;
}

// Rank how well a desktop entry's exec basename matches a resolved browser
// brand, lowest is best. Used to pick a single canonical .desktop file when
// several point at the same browser install (e.g. "zen.desktop" and a
// second "zen-browser.desktop" both launching /opt/zen-browser-bin/zen-bin).
int desktopAppRank(const DesktopApp &app, const QString &brand)
{
    const QString execBase = QFileInfo(firstToken(app.execLine)).fileName().toLower();
    const QString brandLower = brand.toLower();
    if (execBase == brandLower) {
        return 0;
    }
    if (execBase.startsWith(brandLower)) {
        return 1;
    }
    if (execBase.contains(brandLower)) {
        return 2;
    }
    return 3;
}

bool isBetterDesktopApp(const DesktopApp &candidate, const DesktopApp &current, const QString &brand)
{
    const int candidateRank = desktopAppRank(candidate, brand);
    const int currentRank = desktopAppRank(current, brand);
    if (candidateRank != currentRank) {
        return candidateRank < currentRank;
    }
    if (candidate.id.size() != current.id.size()) {
        return candidate.id.size() < current.id.size();
    }
    return candidate.id.compare(current.id, Qt::CaseInsensitive) < 0;
}

struct Fingerprint {
    Engine engine = Engine::Generic;
    QString dataDir;
    QString brand;
};

Fingerprint fingerprint(const DesktopApp &app, const DiscoveryPaths &paths)
{
    Fingerprint fp;
    const QString exec = firstToken(app.execLine).toLower();
    const QString name = app.name.toLower();
    const QString id = app.id.toLower();
    const QString blob = exec + QLatin1Char(' ') + name + QLatin1Char(' ') + id + QLatin1Char(' ') + app.wmClass.toLower();

    // A Flatpak-packaged browser's real profile store lives under
    // ~/.var/app/<app-id>/... rather than the native paths the brand
    // checks below assume; when Exec= itself invokes flatpak, read the
    // app id straight out of it so those candidate lists can add it.
    const bool isFlatpak = QFileInfo(exec).fileName() == QLatin1String("flatpak");
    const QString flatpakId = isFlatpak ? flatpakAppId(execPrefix(app.execLine).args) : QString();

    auto bestGeckoDataDir = [](const QStringList &candidates) -> QString {
        // Prefer the candidate whose profiles.ini exists and whose Profile*
        // entries point at directories that are actually present on disk;
        // that is the data dir the browser is really using. Fall back to
        // the first candidate only when none of them qualify.
        QString best;
        int bestScore = -1;
        for (const auto &c : candidates) {
            const QString ini = c + QStringLiteral("/profiles.ini");
            if (!QFile::exists(ini)) {
                continue;
            }
            QSettings s(ini, QSettings::IniFormat);
            int score = 0;
            const auto iniGroups = s.childGroups();
            for (const auto &g : iniGroups) {
                if (!g.startsWith(QLatin1String("Profile"))) {
                    continue;
                }
                s.beginGroup(g);
                const QString path = s.value(QStringLiteral("Path")).toString();
                const bool relative = s.value(QStringLiteral("IsRelative"), 1).toInt() == 1;
                s.endGroup();
                if (path.isEmpty()) {
                    continue;
                }
                const QString abs = relative ? c + QLatin1Char('/') + path : path;
                if (QDir(abs).exists()) {
                    ++score;
                }
            }
            if (score > bestScore) {
                bestScore = score;
                best = c;
            }
        }
        if (!best.isEmpty()) {
            return best;
        }
        return candidates.isEmpty() ? QString() : candidates.first();
    };
    // Per-OS profile-root table. The linux candidates reproduce the list
    // each brand check used before the table existed; the mac candidates
    // are relative to applicationSupport (~/Library/Application Support)
    // and are consulted only when applicationSupport is set, so Linux
    // output is unchanged whether or not the row carries mac entries.
    // Rule order is the brand-match order the old if-chain had: more
    // specific needles first (zen before firefox so Zen-named forks win,
    // brave's beta bundle id before plain brave, chrome before chromium),
    // first match wins.
    enum class RootBase { ConfigHome, Home };
    struct RootCandidate {
        RootBase base;
        const char *rel;
    };
    struct RootRule {
        std::initializer_list<const char *> needles; // all must appear in the blob
        const char *antiNeedle = nullptr;            // vetoes the rule when present in the blob
        Engine engine = Engine::Generic;
        const char *brand = "";
        // Brand shown for macOS bundles only. Brave Beta is still
        // fingerprinted as "Brave" on Linux: nothing there distinguishes
        // it, and Linux output must not change.
        const char *macBrand = nullptr;
        std::initializer_list<RootCandidate> linux;
        std::initializer_list<const char *> mac; // relative to applicationSupport
        const char *flatpakRel = "";             // ~/.var/app/<id>/<flatpakRel>, gecko only
    };
    static const RootRule rootRules[] = {
        {{"zen"}, nullptr, Engine::Gecko, "Zen", nullptr,
         {{RootBase::ConfigHome, "zen"}, {RootBase::Home, ".zen"}}, {"zen", "Zen"}, ".zen"},
        {{"librewolf"}, nullptr, Engine::Gecko, "LibreWolf", nullptr,
         {{RootBase::ConfigHome, "librewolf"}, {RootBase::Home, ".librewolf"}}, {"LibreWolf"},
         ".librewolf"},
        {{"floorp"}, nullptr, Engine::Gecko, "Floorp", nullptr,
         {{RootBase::Home, ".floorp"}, {RootBase::ConfigHome, "floorp"}}, {"Floorp"}, ".floorp"},
        {{"waterfox"}, nullptr, Engine::Gecko, "Waterfox", nullptr,
         {{RootBase::Home, ".waterfox"}}, {"Waterfox"}, ".waterfox"},
        {{"firefox"}, "pwa", Engine::Gecko, "Firefox", nullptr,
         {{RootBase::ConfigHome, "mozilla/firefox"}, {RootBase::Home, ".mozilla/firefox"}},
         {"Firefox"}, ".mozilla/firefox"},
        {{"brave", "browser.beta"}, nullptr, Engine::Chromium, "Brave", "Brave Beta",
         {{RootBase::ConfigHome, "BraveSoftware/Brave-Browser"}}, {"BraveSoftware/Brave-Browser-Beta"}},
        {{"brave"}, nullptr, Engine::Chromium, "Brave", nullptr,
         {{RootBase::ConfigHome, "BraveSoftware/Brave-Browser"}}, {"BraveSoftware/Brave-Browser"}},
        {{"google-chrome"}, nullptr, Engine::Chromium, "Chrome", nullptr,
         {{RootBase::ConfigHome, "google-chrome"}}, {"Google/Chrome"}},
        {{"google chrome"}, nullptr, Engine::Chromium, "Chrome", nullptr,
         {{RootBase::ConfigHome, "google-chrome"}}, {"Google/Chrome"}},
        {{"chromium"}, nullptr, Engine::Chromium, "Chromium", nullptr,
         {{RootBase::ConfigHome, "chromium"}}, {"Chromium"}},
        {{"microsoft-edge"}, nullptr, Engine::Chromium, "Edge", nullptr,
         {{RootBase::ConfigHome, "microsoft-edge"}}, {"Microsoft Edge"}},
        {{"microsoft edge"}, nullptr, Engine::Chromium, "Edge", nullptr,
         {{RootBase::ConfigHome, "microsoft-edge"}}, {"Microsoft Edge"}},
        {{"msedge"}, nullptr, Engine::Chromium, "Edge", nullptr,
         {{RootBase::ConfigHome, "microsoft-edge"}}, {"Microsoft Edge"}},
        {{"vivaldi"}, nullptr, Engine::Chromium, "Vivaldi", nullptr,
         {{RootBase::ConfigHome, "vivaldi"}}, {"Vivaldi"}},
        {{"opera"}, nullptr, Engine::Chromium, "Opera", nullptr,
         {{RootBase::ConfigHome, "opera"}}, {"com.operasoftware.Opera"}},
        {{"thorium"}, nullptr, Engine::Chromium, "Thorium", nullptr,
         {{RootBase::ConfigHome, "thorium"}}, {"Thorium"}},
    };

    const bool macRoots = !paths.applicationSupport.isEmpty();
    auto candidatesFor = [&](const RootRule &r) {
        QStringList out;
        if (macRoots) {
            for (const char *rel : r.mac) {
                out << paths.applicationSupport + QLatin1Char('/') + QLatin1String(rel);
            }
            return out;
        }
        for (const auto &c : r.linux) {
            out << (c.base == RootBase::ConfigHome ? paths.configHome : paths.home)
                    + QLatin1Char('/') + QLatin1String(c.rel);
        }
        return out;
    };

    auto gecko = [&](const QString &brand, const QStringList &nativeCandidates, const QString &flatpakRelDir) {
        fp.engine = Engine::Gecko;
        fp.brand = brand;
        QStringList candidates = nativeCandidates;
        if (!flatpakId.isEmpty()) {
            // This desktop entry's own Exec runs flatpak, so flatpakId came
            // straight out of it: the Flatpak profile store is what this
            // exact browser install actually uses, and goes first so it
            // wins over a same-brand native install that happens to
            // coexist. A native (non-Flatpak) desktop entry never reaches
            // here with a non-empty flatpakId, since the id can only be
            // read out of a flatpak Exec= line in the first place;
            // guessing one for a native entry would be speculative.
            candidates.prepend(paths.home + QStringLiteral("/.var/app/") + flatpakId + QLatin1Char('/') + flatpakRelDir);
        }
        fp.dataDir = bestGeckoDataDir(candidates);
    };
    auto chromium = [&](const QString &brand, const QString &relDir) {
        fp.engine = Engine::Chromium;
        fp.brand = brand;
        // Mirrors the Gecko flatpakId check above: a Chromium-based
        // browser packaged as a Flatpak keeps its profile directory under
        // ~/.var/app/<app-id>/config/... instead of ~/.config/....
        // relDir is relative to configHome on Linux and to
        // applicationSupport on macOS; the flatpak branch only ever runs
        // on Linux (a bundle has no flatpak Exec= to read an id from), so
        // it is always handed the Linux rel.
        if (flatpakId.isEmpty()) {
            fp.dataDir = (macRoots ? paths.applicationSupport : paths.configHome) + QLatin1Char('/') + relDir;
        } else {
            fp.dataDir = paths.home + QStringLiteral("/.var/app/") + flatpakId + QStringLiteral("/config/") + relDir;
        }
    };

    for (const RootRule &r : rootRules) {
        bool matched = true;
        for (const char *needle : r.needles) {
            if (!blob.contains(QLatin1String(needle))) {
                matched = false;
                break;
            }
        }
        if (!matched || (r.antiNeedle && blob.contains(QLatin1String(r.antiNeedle)))) {
            continue;
        }
        const QString brand =
            QString::fromLatin1(macRoots && r.macBrand ? r.macBrand : r.brand);
        if (r.engine == Engine::Gecko) {
            gecko(brand, candidatesFor(r), QLatin1String(r.flatpakRel));
        } else if (r.engine == Engine::Chromium) {
            const QString relDir = macRoots && r.mac.size()
                ? QLatin1String(*r.mac.begin())
                : QLatin1String(r.linux.begin()->rel);
            chromium(brand, relDir);
        } else {
            fp.engine = Engine::Generic;
            fp.brand = brand;
        }
        return fp;
    }
    fp.engine = Engine::Generic;
    fp.brand = app.name;
    return fp;
}

bool junkProfilePath(const QString &path)
{
    const QString p = path.toLower();
    return p.contains(QLatin1String("backup"))
        || p.contains(QLatin1String("crashrecovery"))
        || p.contains(QLatin1String("back-ovfs"))
        || p.contains(QLatin1String("ovfs"));
}

bool junkProfileName(const QString &name)
{
    return name.trimmed().compare(QLatin1String("crash"), Qt::CaseInsensitive) == 0;
}

// True when a Gecko profile label is an internal placeholder (the kind the
// profile manager invents, e.g. "default-release-1", "Default Profile", or
// Zen's "Default (release)") rather than something a person chose.
bool isGenericProfileLabel(const QString &label)
{
    static const QRegularExpression generic(
        QStringLiteral("^default(\\s*\\(release\\)|\\s*profile|-release(-\\d+)?)?$"),
        QRegularExpression::CaseInsensitiveOption);
    return generic.match(label.trimmed()).hasMatch();
}

// Pick the best human label for a Gecko profile row. `key` is the profile
// directory's basename (e.g. "qq35x6ld.Work"). Never invents a name from
// browsing data; only re-labels using information already in profiles.ini.
QString geckoProfileLabel(const QString &name, const QString &key, bool isInstallDefault)
{
    const QString base = name.isEmpty() ? key : name;
    if (!isGenericProfileLabel(base)) {
        return base;
    }
    if (isInstallDefault) {
        return QStringLiteral("Default");
    }
    const int dot = key.indexOf(QLatin1Char('.'));
    if (dot >= 0) {
        const QString suffix = key.mid(dot + 1);
        if (!suffix.isEmpty() && !isGenericProfileLabel(suffix)) {
            return suffix;
        }
    }
    return base;
}

// Maps a Firefox/Zen contextual-identity color name to the color it renders
// as in the browser's container UI. Colors introduced after this list was
// written (e.g. "cyan") are left as the default QColor deliberately, per
// product decision: an unmapped color is not worth guessing at.
QColor containerColor(const QString &name)
{
    static const QHash<QString, QColor> colors = {
        {QStringLiteral("blue"), QColor(0x37, 0xAD, 0xFF)},
        {QStringLiteral("turquoise"), QColor(0x00, 0xC7, 0x9A)},
        {QStringLiteral("green"), QColor(0x51, 0xCD, 0x00)},
        {QStringLiteral("yellow"), QColor(0xFF, 0xCB, 0x00)},
        {QStringLiteral("orange"), QColor(0xFF, 0x9F, 0x00)},
        {QStringLiteral("red"), QColor(0xFF, 0x61, 0x3D)},
        {QStringLiteral("pink"), QColor(0xFF, 0x4B, 0xDA)},
        {QStringLiteral("purple"), QColor(0xAF, 0x51, 0xF5)},
        {QStringLiteral("toolbar"), QColor(0x73, 0x73, 0x73)},
    };
    return colors.value(name.toLower());
}

// Only a handful of stock contextual identities ship without an explicit
// "name"; they're addressed by l10nId instead. Any l10nId outside this set
// is an identity Lane doesn't recognize (a future Firefox default, or a
// corrupted entry) and is skipped rather than shown as a raw key.
QString containerL10nName(const QString &l10nId)
{
    static const QHash<QString, QString> names = {
        {QStringLiteral("user-context-personal"), QStringLiteral("Personal")},
        {QStringLiteral("user-context-work"), QStringLiteral("Work")},
        {QStringLiteral("user-context-banking"), QStringLiteral("Banking")},
        {QStringLiteral("user-context-shopping"), QStringLiteral("Shopping")},
    };
    return names.value(l10nId);
}

enum class ContainerHandlerStatus { Present, Absent, Unknown };

// A profile's containers.json lists every contextual identity Firefox/Zen
// ever created, whether or not anything will act on ext+container links.
// Without a protocol-handler extension (Open URL in Container, Default
// Container Handler, ...) launching "ext+container:..." just opens a blank
// tab, so containers are only offered as targets where the browser can
// actually honor them. extensions.json (the startup cache Firefox/Zen
// write for every installed add-on) is enough to tell: it embeds each
// add-on's id, and known handlers are matched by id there. A literal
// "ext+container" match covers any handler whose cached metadata mentions
// the protocol directly.
ContainerHandlerStatus containerHandlerStatus(const QString &profileDir)
{
    // Open URL in Container (addons.mozilla.org). Other handlers (e.g.
    // Default Container Handler) are matched via the literal scan below.
    static const QByteArray kOpenUrlInContainerId = QByteArrayLiteral("{f069aec0-43c5-4bbf-b6b4-df95c4326b98}");

    QFile f(profileDir + QStringLiteral("/extensions.json"));
    if (!f.open(QIODevice::ReadOnly)) {
        return ContainerHandlerStatus::Unknown;
    }
    const QByteArray data = f.readAll();
    if (data.contains(kOpenUrlInContainerId) || data.contains(QByteArrayLiteral("ext+container"))) {
        return ContainerHandlerStatus::Present;
    }
    return ContainerHandlerStatus::Absent;
}

// Builds the container targets for one already-discovered real (non-private)
// Gecko profile target. `profile` must already have its final id, exec,
// icon and profileDir set.
QList<Target> geckoContainers(const Target &profile, const QStringList &argPrefix, const QString &launchProfileDir)
{
    QList<Target> out;
    if (profile.profileDir.isEmpty()) {
        return out;
    }
    QFile f(profile.profileDir + QStringLiteral("/containers.json"));
    if (!f.open(QIODevice::ReadOnly)) {
        return out;
    }
    const auto doc = QJsonDocument::fromJson(f.readAll());
    const auto identities = doc.object().value(QStringLiteral("identities")).toArray();

    const auto handlerStatus = containerHandlerStatus(profile.profileDir);
    if (handlerStatus == ContainerHandlerStatus::Absent) {
        // extensions.json was readable and named no known handler: the
        // browser can't act on ext+container links here, so don't offer any.
        return out;
    }
    if (handlerStatus == ContainerHandlerStatus::Unknown) {
        // extensions.json couldn't be inspected at all. Fall back to a
        // weaker signal: a person who never made a custom container almost
        // certainly never installed a protocol-handler extension either.
        const bool hasCustomName = std::any_of(identities.begin(), identities.end(), [](const QJsonValue &v) {
            const auto id = v.toObject();
            return id.value(QStringLiteral("public")).toBool()
                && !id.value(QStringLiteral("name")).toString().trimmed().isEmpty();
        });
        if (!hasCustomName) {
            return out;
        }
    }

    const QString browserName = profile.displayName();
    QSet<int> seenIds;
    // Zen and Firefox can accumulate duplicate identities in
    // containers.json that differ only in userContextId (a stale entry
    // left over when a container is re-created). Two targets named
    // "Dev" in one profile are indistinguishable to the user, so keep
    // the first in file order and skip the rest by name,
    // case-insensitively.
    QSet<QString> seenNames;
    QStringList droppedDupes;
    for (const auto &v : identities) {
        const auto id = v.toObject();
        if (!id.value(QStringLiteral("public")).toBool()) {
            continue;
        }
        const int userContextId = id.value(QStringLiteral("userContextId")).toInt();
        if (seenIds.contains(userContextId)) {
            continue;
        }
        QString name = id.value(QStringLiteral("name")).toString().trimmed();
        if (name.isEmpty()) {
            name = containerL10nName(id.value(QStringLiteral("l10nId")).toString());
        }
        if (name.isEmpty() || name.startsWith(QLatin1String("userContextIdInternal"))) {
            continue;
        }
        if (seenNames.contains(name.toLower())) {
            droppedDupes << name;
            continue;
        }
        seenIds.insert(userContextId);
        seenNames.insert(name.toLower());

        Target t;
        t.id = profile.id + QStringLiteral(":container:") + QString::number(userContextId);
        t.kind = Kind::Container;
        t.engine = profile.engine;
        t.name = name;
        t.browserName = browserName;
        t.subtitle = browserName + QStringLiteral(" · ") + name;
        t.exec = profile.exec;
        const QString encodedName = QString::fromUtf8(QUrl::toPercentEncoding(name));
        t.args = argPrefix + QStringList{
            QStringLiteral("--profile"),
            launchProfileDir,
            QStringLiteral("--new-tab"),
            QStringLiteral("ext+container:name=") + encodedName + QStringLiteral("&url=$urlEncoded"),
        };
        t.icon = profile.icon;
        t.bundleId = profile.bundleId;
        t.bundlePath = profile.bundlePath;
        t.profileKey = profile.profileKey;
        t.profileDir = profile.profileDir;
        t.containerId = userContextId;
        t.containerName = name;
        t.color = containerColor(id.value(QStringLiteral("color")).toString());
        out.append(t);
    }
    if (!droppedDupes.isEmpty()) {
        qInfo() << "Lane:" << profile.displayName() << "has duplicate container names in containers.json;"
                << "keeping the first of each and hiding" << droppedDupes;
    }
    return out;
}

QList<Target> geckoProfiles(const DesktopApp &app, const Fingerprint &fp, const DiscoveryPaths &paths)
{
    QList<Target> out;
    const auto prefix = execPrefix(app.execLine);
    const QStringList argPrefix = flatpakPrefixArgs(prefix);
    const QString flatpakId = argPrefix.isEmpty() ? QString() : flatpakAppId(argPrefix);
    // Inside the Flatpak sandbox the profile store is mounted at its
    // persist path (~/.zen for Zen), not at the host's
    // ~/.var/app/<app-id>/... spelling. The running instance registers
    // its remoting name from the in-sandbox spelling, so --profile must
    // use it too: with the host spelling the launch can't find the
    // running instance and Zen reports "already running but not
    // responding". profileDir keeps the host path for Lane's own file
    // reads; only the argv spelling is translated.
    // Assumes the app's default Flatpak persist layout (in-sandbox mount at
    // ~/<subdir>, matching the host's ~/.var/app/<id>/<subdir>). That
    // assumption is exact for every gecko flatpakRelDir Lane registers —
    // `flatpak override --persist=FILENAME` only bind-mounts the homedir
    // path to the same subpath, so it cannot remap the mount elsewhere.
    // The real residual risk is a profile store not reached through a
    // persist bind mount at all; revisit only if users report the
    // "not responding" dialog recurring on a flatpak browser.
    auto launchProfileDir = [&](const QString &hostDir) {
        if (flatpakId.isEmpty()) {
            return hostDir;
        }
        const QString base = paths.home + QStringLiteral("/.var/app/") + flatpakId + QLatin1Char('/');
        return hostDir.startsWith(base) ? paths.home + QLatin1Char('/') + hostDir.mid(base.size()) : hostDir;
    };
    const QString iniPath = fp.dataDir + QStringLiteral("/profiles.ini");
    if (!QFile::exists(iniPath)) {
        Target t;
        t.id = QStringLiteral("browser:") + app.id + QStringLiteral(":default");
        t.kind = Kind::BrowserProfile;
        t.engine = Engine::Gecko;
        t.name = QStringLiteral("Default");
        t.browserName = fp.brand.isEmpty() ? app.name : fp.brand;
        t.subtitle = t.browserName;
        t.exec = prefix.program;
        t.args = argPrefix + QStringList{QStringLiteral("--new-tab"), QStringLiteral("$url")};
        t.icon = app.icon;
        t.bundleId = app.bundleId;
        t.bundlePath = app.bundlePath;
        t.isBrowserDefault = true;
        out.append(t);
        return out;
    }

    QSettings ini(iniPath, QSettings::IniFormat);
    const auto groups = ini.childGroups();
    // Some machines have several Firefox/Zen installs sharing one profile
    // store (e.g. release + ESR + Nightly), each with its own [InstallXXXX]
    // section and its own default profile. A single desktop entry can't be
    // matched back to one of those install ids, so an Install-section
    // default is only authoritative when every install agrees on it;
    // otherwise fall back to the legacy single-value Default=1 marker,
    // which Firefox itself keeps around for exactly this ambiguous case.
    QSet<QString> installDefaults;
    for (const auto &g : groups) {
        if (g.startsWith(QLatin1String("Install"))) {
            ini.beginGroup(g);
            const QString def = ini.value(QStringLiteral("Default")).toString();
            ini.endGroup();
            if (!def.isEmpty()) {
                installDefaults.insert(def);
            }
        }
    }
    const QString defaultPath = installDefaults.size() == 1 ? *installDefaults.begin() : QString();
    const bool hasUnambiguousInstallDefault = !defaultPath.isEmpty();

    struct Row {
        QString name;
        QString path;
        bool isDefault = false;
    };
    QList<Row> rows;
    for (const auto &g : groups) {
        if (!g.startsWith(QLatin1String("Profile"))) {
            continue;
        }
        ini.beginGroup(g);
        Row r;
        r.name = ini.value(QStringLiteral("Name")).toString();
        r.path = ini.value(QStringLiteral("Path")).toString();
        r.isDefault = ini.value(QStringLiteral("Default")).toInt() == 1;
        const bool relative = ini.value(QStringLiteral("IsRelative"), 1).toInt() == 1;
        ini.endGroup();
        if (r.path.isEmpty() || junkProfilePath(r.path) || junkProfileName(r.name)) {
            continue;
        }
        if (relative) {
            r.path = fp.dataDir + QLatin1Char('/') + r.path;
        }
        if (junkProfilePath(r.path)) {
            continue;
        }
        // Skip rows profiles.ini still lists but that are gone from disk, and
        // rows that were never actually launched (no prefs.js yet).
        if (!QDir(r.path).exists() || !QFile::exists(r.path + QStringLiteral("/prefs.js"))) {
            continue;
        }
        rows.append(r);
    }

    for (const auto &r : rows) {
        Target t;
        const QString key = QFileInfo(r.path).fileName();
        // With an Install section present (Firefox/Zen's per-install default
        // marker), that marker is authoritative; a stale per-profile
        // Default=1 left over from before multi-install support must not
        // also claim to be the default.
        const bool isInstallDefault = hasUnambiguousInstallDefault
            ? (key == defaultPath || key == QFileInfo(defaultPath).fileName())
            : r.isDefault;
        t.id = QStringLiteral("browser:") + app.id + QLatin1Char(':') + key;
        t.kind = Kind::BrowserProfile;
        t.engine = Engine::Gecko;
        t.name = geckoProfileLabel(r.name, key, isInstallDefault);
        t.browserName = fp.brand.isEmpty() ? app.name : fp.brand;
        t.subtitle = t.browserName + QStringLiteral(" · ") + t.name;
        t.exec = prefix.program;
        t.args = argPrefix
            + QStringList{QStringLiteral("--profile"), launchProfileDir(r.path), QStringLiteral("--new-tab"), QStringLiteral("$url")};
        t.icon = app.icon;
        t.bundleId = app.bundleId;
        t.bundlePath = app.bundlePath;
        t.profileKey = r.name;
        t.profileDir = r.path;
        t.isBrowserDefault = isInstallDefault;
        out.append(t);
        out.append(geckoContainers(t, argPrefix, launchProfileDir(r.path)));

        Target priv = t;
        priv.id += QStringLiteral(":private");
        priv.name = t.name + QStringLiteral(" (Private)");
        priv.subtitle = t.browserName + QStringLiteral(" · Private");
        priv.args = argPrefix
            + QStringList{QStringLiteral("--profile"), launchProfileDir(r.path), QStringLiteral("--private-window"), QStringLiteral("$url")};
        priv.incognito = true;
        out.append(priv);
    }
    return out;
}

QString chromiumProfileIcon(const QString &dataDir, const QString &key)
{
    const QStringList candidates = {
        dataDir + QLatin1Char('/') + key + QStringLiteral("/Google Profile Picture.png"),
        dataDir + QLatin1Char('/') + key + QStringLiteral("/profile_picture.png"),
        dataDir + QStringLiteral("/Avatars/") + key + QStringLiteral(".png"),
    };
    for (const auto &c : candidates) {
        if (QFile::exists(c)) {
            return c;
        }
    }
    return {};
}

QList<Target> chromiumProfiles(const DesktopApp &app, const Fingerprint &fp)
{
    QList<Target> out;
    const auto prefix = execPrefix(app.execLine);
    const QStringList argPrefix = flatpakPrefixArgs(prefix);
    const QString localState = fp.dataDir + QStringLiteral("/Local State");
    QJsonObject cache;
    if (QFile::exists(localState)) {
        QFile f(localState);
        if (f.open(QIODevice::ReadOnly)) {
            const auto doc = QJsonDocument::fromJson(f.readAll());
            cache = doc.object().value(QStringLiteral("profile")).toObject().value(QStringLiteral("info_cache")).toObject();
        }
    }

    auto addProfile = [&](const QString &key, const QString &name, bool isDefault) {
        if (!QDir(fp.dataDir + QLatin1Char('/') + key).exists() && key != QLatin1String("Default")) {
            // Still allow Default even if the folder is missing; chromium creates it.
            return;
        }
        Target t;
        t.id = QStringLiteral("browser:") + app.id + QLatin1Char(':') + key;
        t.kind = Kind::BrowserProfile;
        t.engine = Engine::Chromium;
        t.name = name.isEmpty() ? key : name;
        t.browserName = fp.brand.isEmpty() ? app.name : fp.brand;
        t.subtitle = t.browserName + QStringLiteral(" · ") + t.name;
        t.exec = prefix.program;
        t.args = argPrefix
            + QStringList{QStringLiteral("--profile-directory=") + key, QStringLiteral("--new-tab"), QStringLiteral("$url")};
        const QString pic = chromiumProfileIcon(fp.dataDir, key);
        t.icon = pic.isEmpty() ? app.icon : pic;
        t.bundleId = app.bundleId;
        t.bundlePath = app.bundlePath;
        t.profileKey = key;
        t.profileDir = fp.dataDir + QLatin1Char('/') + key;
        t.isBrowserDefault = isDefault || key == QLatin1String("Default");
        out.append(t);

        Target inc = t;
        inc.id += QStringLiteral(":incognito");
        inc.name = t.name + QStringLiteral(" (Incognito)");
        inc.subtitle = t.browserName + QStringLiteral(" · Incognito");
        // Edge on macOS ignores --incognito (it opens a normal window,
        // verified on the port machine); its private flag is --inprivate.
        // Linux Edge keeps --incognito: the flag check is keyed on
        // bundleId, which only .app targets carry.
        const QString privateFlag = app.bundleId == QLatin1String("com.microsoft.edgemac")
            ? QStringLiteral("--inprivate")
            : QStringLiteral("--incognito");
        inc.args = argPrefix
            + QStringList{QStringLiteral("--profile-directory=") + key, privateFlag, QStringLiteral("$url")};
        inc.incognito = true;
        inc.isBrowserDefault = false;
        out.append(inc);
    };

    if (cache.isEmpty()) {
        addProfile(QStringLiteral("Default"), QStringLiteral("Default"), true);
    } else {
        for (auto it = cache.begin(); it != cache.end(); ++it) {
            const auto info = it.value().toObject();
            addProfile(it.key(), info.value(QStringLiteral("name")).toString(), it.key() == QLatin1String("Default"));
        }
    }

    if (fp.brand == QLatin1String("Brave")) {
        Target tor;
        tor.id = QStringLiteral("browser:") + app.id + QStringLiteral(":tor");
        tor.kind = Kind::BrowserProfile;
        tor.engine = Engine::Chromium;
        tor.name = QStringLiteral("Tor");
        tor.browserName = QStringLiteral("Brave");
        tor.subtitle = QStringLiteral("Brave · Tor");
        tor.exec = prefix.program;
        tor.args = argPrefix + QStringList{QStringLiteral("--tor"), QStringLiteral("$url")};
        tor.icon = app.icon;
        tor.bundleId = app.bundleId;
        tor.bundlePath = app.bundlePath;
        tor.incognito = true;
        out.append(tor);
    }
    return out;
}

QList<Target> genericBrowser(const DesktopApp &app, const Fingerprint &fp)
{
    const auto prefix = execPrefix(app.execLine);
    const QStringList argPrefix = flatpakPrefixArgs(prefix);
    Target t;
    t.id = QStringLiteral("browser:") + app.id + QStringLiteral(":default");
    t.kind = Kind::BrowserProfile;
    t.engine = Engine::Generic;
    t.name = QStringLiteral("Default");
    t.browserName = fp.brand.isEmpty() ? app.name : fp.brand;
    t.subtitle = t.browserName;
    t.exec = prefix.program;
    t.args = argPrefix + QStringList{QStringLiteral("$url")};
    t.icon = app.icon;
    t.bundleId = app.bundleId;
    t.bundlePath = app.bundlePath;
    t.isBrowserDefault = true;
    return {t};
}

QList<Target> discoverPwas(const DiscoveryPaths &paths)
{
    QList<Target> out;
    const QString cfgPath = paths.dataHome + QStringLiteral("/firefoxpwa/config.json");
    QFile f(cfgPath);
    if (!f.open(QIODevice::ReadOnly)) {
        return out;
    }
    const auto doc = QJsonDocument::fromJson(f.readAll());
    const auto sites = doc.object().value(QStringLiteral("sites")).toObject();
    const QString exe = QStandardPaths::findExecutable(QStringLiteral("firefoxpwa"));
    for (auto it = sites.begin(); it != sites.end(); ++it) {
        const auto site = it.value().toObject();
        const auto manifest = site.value(QStringLiteral("manifest")).toObject();
        const auto cfg = site.value(QStringLiteral("config")).toObject();
        Target t;
        t.id = QStringLiteral("pwa:") + it.key();
        t.kind = Kind::Pwa;
        t.engine = Engine::Pwa;
        t.pwaUlid = it.key();
        t.name = manifest.value(QStringLiteral("name")).toString();
        if (t.name.isEmpty()) {
            t.name = cfg.value(QStringLiteral("name")).toString();
        }
        for (const auto &dir : paths.applicationDirs) {
            QFile desk(dir + QStringLiteral("/FFPWA-") + it.key() + QStringLiteral(".desktop"));
            if (!desk.open(QIODevice::ReadOnly)) {
                continue;
            }
            while (!desk.atEnd()) {
                const QString line = QString::fromUtf8(desk.readLine()).trimmed();
                if (line.startsWith(QLatin1String("Name="))) {
                    t.name = line.section(QLatin1Char('='), 1);
                    break;
                }
            }
            break;
        }
        if (t.name.isEmpty()) {
            t.name = it.key();
        }
        t.browserName = QStringLiteral("PWA");
        t.subtitle = QStringLiteral("App");
        t.pwaScope = manifest.value(QStringLiteral("scope")).toString();
        if (t.pwaScope.isEmpty()) {
            t.pwaScope = manifest.value(QStringLiteral("start_url")).toString();
        }
        t.exec = exe.isEmpty() ? QStringLiteral("firefoxpwa") : exe;
        t.args = {QStringLiteral("site"), QStringLiteral("launch"), it.key(), QStringLiteral("--url"), QStringLiteral("$url")};
        t.icon = QStringLiteral("FFPWA-") + it.key();
        out.append(t);
    }
    return out;
}

// Reads one plist value node with the cursor on its start element.
// Scalar types Lane does not need (integer, real, date, data, true,
// false) are consumed but produce an invalid QVariant.
QVariant readPlistValue(QXmlStreamReader &xml)
{
    const QStringView tag = xml.name();
    if (tag == QLatin1String("dict")) {
        QVariantMap map;
        while (xml.readNextStartElement()) {
            if (xml.name() != QLatin1String("key")) {
                xml.skipCurrentElement();
                continue;
            }
            const QString key = xml.readElementText();
            if (!xml.readNextStartElement()) {
                break;
            }
            map.insert(key, readPlistValue(xml));
        }
        return map;
    }
    if (tag == QLatin1String("array")) {
        QVariantList list;
        while (xml.readNextStartElement()) {
            list.append(readPlistValue(xml));
        }
        return list;
    }
    if (tag == QLatin1String("string")) {
        return xml.readElementText();
    }
    xml.skipCurrentElement();
    return {};
}

// Parses the top-level <dict> of an XML property list. Returns an empty
// map on any parse error.
QVariantMap parseXmlPlist(const QByteArray &data)
{
    QXmlStreamReader xml(data);
    while (xml.readNextStartElement()) {
        if (xml.name() != QLatin1String("plist")) {
            xml.skipCurrentElement();
            continue;
        }
        if (!xml.readNextStartElement()) {
            break;
        }
        return readPlistValue(xml).toMap();
    }
    return {};
}

// Loads an Info.plist as a QVariantMap. XML plists are parsed directly;
// a binary plist ("bplist" magic) needs conversion through
// /usr/bin/plutil, which exists only on macOS, so elsewhere a binary
// Info.plist means the bundle is skipped rather than failing.
QVariantMap readInfoPlist(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        return {};
    }
    QByteArray data = f.read(8 * 1024 * 1024);
    if (data.startsWith(QByteArrayLiteral("bplist"))) {
#ifdef Q_OS_MACOS
        QProcess plutil;
        plutil.start(QStringLiteral("/usr/bin/plutil"),
                     {QStringLiteral("-convert"), QStringLiteral("xml1"), QStringLiteral("-o"), QStringLiteral("-"),
                      QStringLiteral("--"), path});
        if (!plutil.waitForFinished(10000) || plutil.exitCode() != 0) {
            qDebug() << "Lane: skipping" << path << "- plutil could not convert its binary plist";
            return {};
        }
        data = plutil.readAllStandardOutput();
#else
        qDebug() << "Lane: skipping" << path
                 << "- binary Info.plist conversion needs /usr/bin/plutil (macOS only)";
        return {};
#endif
    }
    return parseXmlPlist(data);
}

// Scans each dir in `dirs` one level deep for .app bundles and turns the
// qualifying ones into DesktopApp entries so the shared Gecko/Chromium/
// generic walkers can build their profile targets. A bundle qualifies
// only when its Info.plist declares http or https in
// CFBundleURLTypes/CFBundleURLSchemes and its CFBundleExecutable resolves
// to a regular executable file inside the bundle's Contents/MacOS (a
// name containing a slash or dot-dot, or a symlink pointing outside the
// bundle, is rejected with a log line). Lane's own bundle id is never a
// target. Results are deduped by bundle id (shorter bundle path wins,
// then lexicographic) and by resolved real path.
QHash<QString, QString> g_macBundlePaths;

QList<DesktopApp> scanMacBundles(const QStringList &dirs)
{
    g_macBundlePaths.clear();
    QList<DesktopApp> apps;
    QHash<QString, int> byId;
    QSet<QString> seenCanonical;
    for (const QString &dir : dirs) {
        const QDir d(dir);
        // QDir::Hidden because Apple ships Safari and other system apps
        // as `chflags hidden` symlinks into the cryptex volume on macOS;
        // without it they never reach the entry list at all.
        const auto entries = d.entryList({QStringLiteral("*.app")},
                                         QDir::Dirs | QDir::Hidden | QDir::NoDotAndDotDot, QDir::Name);
        for (const QString &entry : entries) {
            const QString bundlePath = d.filePath(entry);
            const QString canonicalPath = QFileInfo(bundlePath).canonicalFilePath();
            if (canonicalPath.isEmpty() || seenCanonical.contains(canonicalPath)) {
                continue;
            }

            const QVariantMap plist = readInfoPlist(bundlePath + QStringLiteral("/Contents/Info.plist"));
            const QString bundleId = plist.value(QStringLiteral("CFBundleIdentifier")).toString();
            if (bundleId.isEmpty()) {
                continue;
            }
            if (bundleId.compare(QLatin1String("app.lane.Lane"), Qt::CaseInsensitive) == 0) {
                continue; // Lane is never its own target
            }
            bool handlesWeb = false;
            const auto urlTypes = plist.value(QStringLiteral("CFBundleURLTypes")).toList();
            for (const auto &urlType : urlTypes) {
                const auto schemes = urlType.toMap().value(QStringLiteral("CFBundleURLSchemes")).toList();
                for (const auto &scheme : schemes) {
                    const QString s = scheme.toString().toLower();
                    if (s == QLatin1String("http") || s == QLatin1String("https")) {
                        handlesWeb = true;
                    }
                }
            }
            if (!handlesWeb) {
                continue;
            }

            const QString execName = plist.value(QStringLiteral("CFBundleExecutable")).toString();
            if (execName.isEmpty() || execName.contains(QLatin1Char('/')) || execName.contains(QLatin1Char('\\'))
                || execName == QLatin1String(".") || execName == QLatin1String("..")) {
                qDebug() << "Lane: skipping" << bundlePath
                         << "- CFBundleExecutable is not a bare file name:" << execName;
                continue;
            }
            const QString macosDir = bundlePath + QStringLiteral("/Contents/MacOS");
            const QString canonicalMacosDir = QFileInfo(macosDir).canonicalFilePath();
            const QString execPath = macosDir + QLatin1Char('/') + execName;
            const QFileInfo execInfo(execPath);
            const QString canonicalExec = execInfo.canonicalFilePath();
            if (canonicalMacosDir.isEmpty() || canonicalExec.isEmpty() || !execInfo.isFile() || !execInfo.isExecutable()
                || !(canonicalExec == canonicalMacosDir
                     || canonicalExec.startsWith(canonicalMacosDir + QLatin1Char('/')))) {
                qDebug() << "Lane: skipping" << bundlePath
                         << "- CFBundleExecutable does not resolve to an executable file inside Contents/MacOS:"
                         << execName;
                continue;
            }

            seenCanonical.insert(canonicalPath);
            DesktopApp app;
            app.id = QStringLiteral("macapp:") + bundleId;
            app.name = plist.value(QStringLiteral("CFBundleDisplayName")).toString();
            if (app.name.isEmpty()) {
                app.name = plist.value(QStringLiteral("CFBundleName")).toString();
            }
            if (app.name.isEmpty()) {
                app.name = QFileInfo(bundlePath).completeBaseName();
            }
            app.icon = QStringLiteral("bundle:") + bundleId;
            app.bundleId = bundleId;
            app.bundlePath = bundlePath;
            // The walkers take the program out of execLine via
            // execPrefix(); quoting keeps a bundle path containing spaces
            // (e.g. "Brave Browser.app") a single token.
            app.execLine = QLatin1Char('"') + execPath + QLatin1Char('"');

            const auto it = byId.constFind(bundleId);
            if (it == byId.constEnd()) {
                byId.insert(bundleId, apps.size());
                apps.append(app);
                g_macBundlePaths.insert(bundleId, app.bundlePath);
            } else {
                // Same bundle id installed twice: keep the shorter bundle
                // path, breaking a tie lexicographically.
                const QString &current = apps.at(it.value()).bundlePath;
                if (bundlePath.size() < current.size()
                    || (bundlePath.size() == current.size() && bundlePath < current)) {
                    apps[it.value()] = app;
                    g_macBundlePaths.insert(bundleId, app.bundlePath);
                }
            }
        }
    }
    return apps;
}

QList<Target> actionTargets()
{
    Target copy;
    copy.id = QStringLiteral("action:copy");
    copy.kind = Kind::Action;
    copy.engine = Engine::Action;
    copy.name = QStringLiteral("Copy link");
    copy.browserName = QStringLiteral("Lane");
    copy.subtitle = QStringLiteral("Clipboard");
    copy.icon = QStringLiteral("edit-copy");

    Target mail;
    mail.id = QStringLiteral("action:email");
    mail.kind = Kind::Action;
    mail.engine = Engine::Action;
    mail.name = QStringLiteral("Email link");
    mail.browserName = QStringLiteral("Lane");
    mail.subtitle = QStringLiteral("Mail");
    mail.icon = QStringLiteral("mail-sent");
#ifdef Q_OS_MACOS
    // The launcher builds the mailto open invocation itself via
    // macFixedOpenArgs and ignores args entirely; this string is only
    // what `lane --list` prints, so it names the real binary and carries
    // no leftover Linux argv.
    mail.exec = QStringLiteral("/usr/bin/open");
    mail.args = {QStringLiteral("$url")};
#else
    mail.exec = QStringLiteral("xdg-email");
    mail.args = {QStringLiteral("--body"), QStringLiteral("$url")};
#endif
    return {copy, mail};
}

} // namespace

QString macBundlePathForId(const QString &bundleId)
{
    return g_macBundlePaths.value(bundleId);
}

DiscoveryPaths defaultDiscoveryPaths()
{
    DiscoveryPaths p;
    p.home = QDir::homePath();
    p.configHome = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    p.dataHome = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    p.applicationDirs = QStandardPaths::standardLocations(QStandardPaths::ApplicationsLocation);
#ifdef Q_OS_MACOS
    // ~/Library/Application Support is also GenericDataLocation's
    // writable location on macOS; it doubles as the base for the macOS
    // side of the profile-root table.
    p.applicationSupport = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    p.bundleDirs = {QStringLiteral("/Applications"), p.home + QStringLiteral("/Applications")};
#endif

    // QStandardPaths::ApplicationsLocation does not include Flatpak's own
    // export directories, so a Flatpak-only browser install (no native
    // desktop file anywhere XDG_DATA_DIRS points) is invisible to
    // discovery even though `flatpak run <app-id>` works fine by hand.
    // Flatpak exports desktop files to both the per-user prefix
    // (~/.local/share/flatpak/exports/share/applications, i.e. under
    // dataHome) and the system-wide installation
    // (/var/lib/flatpak/exports/share/applications); append both
    // unconditionally, since scanDesktopFiles() already no-ops on a
    // directory that does not exist.
    const QStringList flatpakExportDirs = {
        p.dataHome + QStringLiteral("/flatpak/exports/share/applications"),
        QStringLiteral("/var/lib/flatpak/exports/share/applications"),
    };
    p.applicationDirs << flatpakExportDirs;

    // If flatpak itself is installed AND at least one export dir exists
    // (so some flatpak app is installed) but neither export dir handed
    // discovery a single browser desktop file, the user's Flatpak
    // browsers will silently be missing from Lane. The latch keeps the
    // warning from repeating on every reload/Rediscover/openSettings
    // pass through this function.
    static bool warnedNoFlatpakBrowser = false;
    const bool anyExportDirExists =
        QDir(p.dataHome + QStringLiteral("/flatpak/exports/share/applications")).exists()
        || QDir(QStringLiteral("/var/lib/flatpak/exports/share/applications")).exists();
    if (!warnedNoFlatpakBrowser && anyExportDirExists
        && !QStandardPaths::findExecutable(QStringLiteral("flatpak")).isEmpty()
        && scanDesktopFiles(flatpakExportDirs).isEmpty()) {
        warnedNoFlatpakBrowser = true;
        qWarning() << "Lane: flatpak is installed but no browser desktop files were found under"
                   << flatpakExportDirs << "- Flatpak browsers may not appear in discovery";
    }

    return p;
}

QList<Target> discoverTargets(const DiscoveryPaths &paths)
{
    QList<Target> out;
    auto apps = scanDesktopFiles(paths.applicationDirs);
    apps += scanMacBundles(paths.bundleDirs);

    // Two desktop files can point at the same browser install and profile
    // store (e.g. an AUR "zen-bin" package shipping zen.desktop while a
    // previous install left zen-browser.desktop behind). Group by what the
    // browser actually is rather than by desktop file, and only ever walk
    // its profiles once, using whichever desktop entry looks most canonical.
    struct Group {
        Fingerprint fp;
        DesktopApp app;
    };
    QHash<QString, Group> groups;
    QStringList groupOrder;
    for (const auto &app : apps) {
        const auto fp = fingerprint(app, paths);
        const QString sig = firstToken(app.execLine) + QLatin1Char('|') + fp.brand + QLatin1Char('|') + fp.dataDir;
        auto it = groups.find(sig);
        if (it == groups.end()) {
            groups.insert(sig, Group{fp, app});
            groupOrder.append(sig);
        } else if (isBetterDesktopApp(app, it.value().app, fp.brand)) {
            it.value().app = app;
        }
    }

    for (const auto &sig : groupOrder) {
        const auto &group = groups.value(sig);
        QList<Target> found;
        switch (group.fp.engine) {
        case Engine::Gecko:
            found = geckoProfiles(group.app, group.fp, paths);
            break;
        case Engine::Chromium:
            found = chromiumProfiles(group.app, group.fp);
            break;
        default:
            found = genericBrowser(group.app, group.fp);
            break;
        }
        out.append(found);
    }
    out.append(discoverPwas(paths));
    out.append(actionTargets());
    return out;
}

QList<Target> applyConfigToTargets(QList<Target> targets, const Config &config)
{
    QSet<QString> hidden(config.hiddenTargetIds.begin(), config.hiddenTargetIds.end());
    for (const auto &custom : config.customTargets) {
        bool exists = false;
        for (const auto &t : targets) {
            if (t.id == custom.id) {
                exists = true;
                break;
            }
        }
        if (!exists) {
            targets.append(custom);
        }
    }

    // Hidden is applied AFTER the custom-target merge: hidden state for a
    // custom target lives only in config.hiddenTargetIds (Target::hidden
    // is never serialized), and on a fresh discoverTargets() pass the
    // custom row does not exist until the merge above — applying hidden
    // first silently un-hid hidden custom targets on every restart.
    // Unconditional assignment, not "set true if present": this must be
    // idempotent under repeated calls on the same already-applied list
    // (Controller::hideTarget()/renameTarget()/moveTarget() reapply
    // config onto m_targets directly), so un-hiding a target has to
    // actually clear a previously-set true, not just leave it stuck.
    for (auto &t : targets) {
        t.hidden = hidden.contains(t.id);
    }

    for (auto &t : targets) {
        t.customName = config.targetAliases.value(t.id);
    }

    if (!config.targetOrder.isEmpty()) {
        QHash<QString, int> orderIndex;
        for (int i = 0; i < config.targetOrder.size(); ++i) {
            orderIndex.insert(config.targetOrder.at(i), i);
        }
        std::stable_sort(targets.begin(), targets.end(), [&orderIndex](const Target &a, const Target &b) {
            const int ia = orderIndex.value(a.id, -1);
            const int ib = orderIndex.value(b.id, -1);
            if (ia == -1 && ib == -1) {
                return false;
            }
            if (ia == -1) {
                return false;
            }
            if (ib == -1) {
                return true;
            }
            return ia < ib;
        });
    }

    return targets;
}

QStringList moveIdAmongSiblings(const QList<Target> &targets, const QString &id, int newIndex)
{
    QStringList fullOrder;
    fullOrder.reserve(targets.size());
    for (const auto &t : targets) {
        fullOrder << t.id;
    }

    // The reorderable set is exactly what the Settings "Targets" list and
    // the picker share: every non-incognito, non-Action target, so a
    // container can sit above a browser and a web app below a custom app.
    // Incognito targets (their own collapsible section, never in the
    // picker) and Action targets (footer/shortcut bound, not rows) are
    // landmarks that keep their slots while everyone else moves past.
    QList<int> siblingSlots;
    QStringList siblingIds;
    for (int i = 0; i < targets.size(); ++i) {
        if (!targets.at(i).incognito && targets.at(i).kind != Kind::Action) {
            siblingSlots.append(i);
            siblingIds << targets.at(i).id;
        }
    }

    const int oldPos = siblingIds.indexOf(id);
    if (oldPos < 0) {
        return fullOrder;
    }

    siblingIds.move(oldPos, qBound(0, newIndex, siblingIds.size() - 1));

    for (int i = 0; i < siblingSlots.size(); ++i) {
        fullOrder[siblingSlots.at(i)] = siblingIds.at(i);
    }

    return fullOrder;
}

} // namespace Lane
