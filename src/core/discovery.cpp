#include "discovery.h"

#include "urlutil.h"

#include <algorithm>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QHash>
#include <QStandardPaths>
#include <QSet>
#include <QRegularExpression>


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
    bool noDisplay = false;
};

QString firstToken(const QString &execLine)
{
    QString s = execLine.trimmed();
    if (s.startsWith(QLatin1Char('"'))) {
        const int end = s.indexOf(QLatin1Char('"'), 1);
        if (end > 0) {
            return s.mid(1, end - 1);
        }
    }
    return s.section(QLatin1Char(' '), 0, 0);
}

// Splits a desktop Exec= line into its program (matching firstToken()'s
// quoting rules) and the remaining tokens with freedesktop field codes
// (%f %F %u %U %d %D %n %N %i %c %k %v %m) and Flatpak's "@@u ... @@"
// file-forwarding markers stripped. For a native browser this prefix is
// empty or a harmless flag; for a Flatpak entry ("flatpak run
// --branch=stable --arch=x86_64 --command=zen app.zen_browser.zen %u")
// it is the "run ... <app-id>" tokens that must stay in front of Lane's
// own --profile/--new-tab args, or the launched process becomes
// "flatpak --profile ..." instead of the browser.
struct ExecPrefix {
    QString program;
    QStringList args;
};

bool isExecFieldCode(const QString &token)
{
    static const QRegularExpression fieldCode(QStringLiteral("^%[fFuUdDnNickvm]$"));
    return token == QLatin1String("@@") || fieldCode.match(token).hasMatch();
}

ExecPrefix execPrefix(const QString &execLine)
{
    ExecPrefix out;
    QString rest = execLine.trimmed();
    if (rest.startsWith(QLatin1Char('"'))) {
        const int end = rest.indexOf(QLatin1Char('"'), 1);
        if (end > 0) {
            out.program = rest.mid(1, end - 1);
            rest = rest.mid(end + 1);
        }
    }
    if (out.program.isEmpty()) {
        const int sp = rest.indexOf(QLatin1Char(' '));
        if (sp < 0) {
            out.program = rest;
            rest.clear();
        } else {
            out.program = rest.left(sp);
            rest = rest.mid(sp);
        }
    }
    const auto tokens = rest.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    for (const auto &t : tokens) {
        if (!isExecFieldCode(t)) {
            out.args << t;
        }
    }
    return out;
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
        if (flatpakId.isEmpty()) {
            fp.dataDir = paths.configHome + QLatin1Char('/') + relDir;
        } else {
            fp.dataDir = paths.home + QStringLiteral("/.var/app/") + flatpakId + QStringLiteral("/config/") + relDir;
        }
    };

    if (blob.contains(QLatin1String("zen"))) {
        gecko(QStringLiteral("Zen"), {paths.configHome + QStringLiteral("/zen"), paths.home + QStringLiteral("/.zen")},
              QStringLiteral(".zen"));
        return fp;
    }
    if (blob.contains(QLatin1String("librewolf"))) {
        gecko(QStringLiteral("LibreWolf"),
              {paths.configHome + QStringLiteral("/librewolf"), paths.home + QStringLiteral("/.librewolf")},
              QStringLiteral(".librewolf"));
        return fp;
    }
    if (blob.contains(QLatin1String("floorp"))) {
        gecko(QStringLiteral("Floorp"), {paths.home + QStringLiteral("/.floorp"), paths.configHome + QStringLiteral("/floorp")},
              QStringLiteral(".floorp"));
        return fp;
    }
    if (blob.contains(QLatin1String("waterfox"))) {
        gecko(QStringLiteral("Waterfox"), {paths.home + QStringLiteral("/.waterfox")}, QStringLiteral(".waterfox"));
        return fp;
    }
    if (blob.contains(QLatin1String("firefox")) && !blob.contains(QLatin1String("pwa"))) {
        gecko(QStringLiteral("Firefox"),
              {paths.configHome + QStringLiteral("/mozilla/firefox"), paths.home + QStringLiteral("/.mozilla/firefox")},
              QStringLiteral(".mozilla/firefox"));
        return fp;
    }
    if (blob.contains(QLatin1String("brave"))) {
        chromium(QStringLiteral("Brave"), QStringLiteral("BraveSoftware/Brave-Browser"));
        return fp;
    }
    if (blob.contains(QLatin1String("google-chrome")) || blob.contains(QLatin1String("google chrome"))) {
        chromium(QStringLiteral("Chrome"), QStringLiteral("google-chrome"));
        return fp;
    }
    if (blob.contains(QLatin1String("chromium"))) {
        chromium(QStringLiteral("Chromium"), QStringLiteral("chromium"));
        return fp;
    }
    if (blob.contains(QLatin1String("microsoft-edge")) || blob.contains(QLatin1String("msedge"))) {
        chromium(QStringLiteral("Edge"), QStringLiteral("microsoft-edge"));
        return fp;
    }
    if (blob.contains(QLatin1String("vivaldi"))) {
        chromium(QStringLiteral("Vivaldi"), QStringLiteral("vivaldi"));
        return fp;
    }
    if (blob.contains(QLatin1String("opera"))) {
        chromium(QStringLiteral("Opera"), QStringLiteral("opera"));
        return fp;
    }
    if (blob.contains(QLatin1String("thorium"))) {
        chromium(QStringLiteral("Thorium"), QStringLiteral("thorium"));
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
QList<Target> geckoContainers(const Target &profile, const QStringList &argPrefix)
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
        seenIds.insert(userContextId);

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
            profile.profileDir,
            QStringLiteral("--new-tab"),
            QStringLiteral("ext+container:name=") + encodedName + QStringLiteral("&url=$urlEncoded"),
        };
        t.icon = profile.icon;
        t.profileKey = profile.profileKey;
        t.profileDir = profile.profileDir;
        t.containerId = userContextId;
        t.containerName = name;
        t.color = containerColor(id.value(QStringLiteral("color")).toString());
        out.append(t);
    }
    return out;
}

QList<Target> geckoProfiles(const DesktopApp &app, const Fingerprint &fp)
{
    QList<Target> out;
    const auto prefix = execPrefix(app.execLine);
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
        t.args = prefix.args + QStringList{QStringLiteral("--new-tab"), QStringLiteral("$url")};
        t.icon = app.icon;
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
        t.args = prefix.args
            + QStringList{QStringLiteral("--profile"), r.path, QStringLiteral("--new-tab"), QStringLiteral("$url")};
        t.icon = app.icon;
        t.profileKey = r.name;
        t.profileDir = r.path;
        t.isBrowserDefault = isInstallDefault;
        out.append(t);
        out.append(geckoContainers(t, prefix.args));

        Target priv = t;
        priv.id += QStringLiteral(":private");
        priv.name = t.name + QStringLiteral(" (Private)");
        priv.subtitle = t.browserName + QStringLiteral(" · Private");
        priv.args = prefix.args
            + QStringList{QStringLiteral("--profile"), r.path, QStringLiteral("--private-window"), QStringLiteral("$url")};
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
        }
        Target t;
        t.id = QStringLiteral("browser:") + app.id + QLatin1Char(':') + key;
        t.kind = Kind::BrowserProfile;
        t.engine = Engine::Chromium;
        t.name = name.isEmpty() ? key : name;
        t.browserName = fp.brand.isEmpty() ? app.name : fp.brand;
        t.subtitle = t.browserName + QStringLiteral(" · ") + t.name;
        t.exec = prefix.program;
        t.args = prefix.args
            + QStringList{QStringLiteral("--profile-directory=") + key, QStringLiteral("--new-tab"), QStringLiteral("$url")};
        const QString pic = chromiumProfileIcon(fp.dataDir, key);
        t.icon = pic.isEmpty() ? app.icon : pic;
        t.profileKey = key;
        t.profileDir = fp.dataDir + QLatin1Char('/') + key;
        t.isBrowserDefault = isDefault || key == QLatin1String("Default");
        out.append(t);

        Target inc = t;
        inc.id += QStringLiteral(":incognito");
        inc.name = t.name + QStringLiteral(" (Incognito)");
        inc.subtitle = t.browserName + QStringLiteral(" · Incognito");
        inc.args = prefix.args
            + QStringList{QStringLiteral("--profile-directory=") + key, QStringLiteral("--incognito"), QStringLiteral("$url")};
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
        tor.args = prefix.args + QStringList{QStringLiteral("--tor"), QStringLiteral("$url")};
        tor.icon = app.icon;
        tor.incognito = true;
        out.append(tor);
    }
    return out;
}

QList<Target> genericBrowser(const DesktopApp &app, const Fingerprint &fp)
{
    const auto prefix = execPrefix(app.execLine);
    Target t;
    t.id = QStringLiteral("browser:") + app.id + QStringLiteral(":default");
    t.kind = Kind::BrowserProfile;
    t.engine = Engine::Generic;
    t.name = QStringLiteral("Default");
    t.browserName = fp.brand.isEmpty() ? app.name : fp.brand;
    t.subtitle = t.browserName;
    t.exec = prefix.program;
    t.args = prefix.args + QStringList{QStringLiteral("$url")};
    t.icon = app.icon;
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
    mail.exec = QStringLiteral("xdg-email");
    mail.args = {QStringLiteral("--body"), QStringLiteral("$url")};
    return {copy, mail};
}

} // namespace

DiscoveryPaths defaultDiscoveryPaths()
{
    DiscoveryPaths p;
    p.home = QDir::homePath();
    p.configHome = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    p.dataHome = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    p.applicationDirs = QStandardPaths::standardLocations(QStandardPaths::ApplicationsLocation);
    return p;
}

QList<Target> discoverTargets(const DiscoveryPaths &paths)
{
    QList<Target> out;
    const auto apps = scanDesktopFiles(paths.applicationDirs);

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
            found = geckoProfiles(group.app, group.fp);
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
    // Unconditional assignment, not "set true if present": this must be
    // idempotent under repeated calls on the same already-applied list
    // (Controller::hideTarget()/renameTarget()/moveTarget() now reapply
    // config onto m_targets directly instead of a freshly discovered
    // list), so un-hiding a target has to actually clear a previously-set
    // true, not just leave it stuck.
    for (auto &t : targets) {
        t.hidden = hidden.contains(t.id);
    }
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

QStringList moveIdAmongSiblings(const QList<Target> &targets, const QString &id, int newIndexInKind)
{
    QStringList fullOrder;
    fullOrder.reserve(targets.size());
    for (const auto &t : targets) {
        fullOrder << t.id;
    }

    int movingIndex = -1;
    for (int i = 0; i < targets.size(); ++i) {
        if (targets.at(i).id == id) {
            movingIndex = i;
            break;
        }
    }
    if (movingIndex < 0) {
        return fullOrder;
    }

    const Kind kind = targets.at(movingIndex).kind;
    const bool incognito = targets.at(movingIndex).incognito;

    QList<int> siblingSlots;
    QStringList siblingIds;
    for (int i = 0; i < targets.size(); ++i) {
        if (targets.at(i).kind == kind && targets.at(i).incognito == incognito) {
            siblingSlots.append(i);
            siblingIds << targets.at(i).id;
        }
    }

    const int oldPos = siblingIds.indexOf(id);
    if (oldPos < 0) {
        return fullOrder;
    }

    siblingIds.move(oldPos, qBound(0, newIndexInKind, siblingIds.size() - 1));

    for (int i = 0; i < siblingSlots.size(); ++i) {
        fullOrder[siblingSlots.at(i)] = siblingIds.at(i);
    }

    return fullOrder;
}

} // namespace Lane
