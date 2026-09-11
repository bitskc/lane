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


namespace Tern
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


bool skipDesktopId(const QString &id)
{
    const QString lower = id.toLower();
    return lower.startsWith(QLatin1String("app.tern"))
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

    auto gecko = [&](const QString &brand, const QStringList &candidates) {
        fp.engine = Engine::Gecko;
        fp.brand = brand;
        for (const auto &c : candidates) {
            if (QFile::exists(c + QStringLiteral("/profiles.ini"))) {
                fp.dataDir = c;
                return;
            }
        }
        if (!candidates.isEmpty()) {
            fp.dataDir = candidates.first();
        }
    };
    auto chromium = [&](const QString &brand, const QString &dir) {
        fp.engine = Engine::Chromium;
        fp.brand = brand;
        fp.dataDir = dir;
    };

    if (blob.contains(QLatin1String("zen"))) {
        gecko(QStringLiteral("Zen"), {paths.configHome + QStringLiteral("/zen"), paths.home + QStringLiteral("/.zen")});
        return fp;
    }
    if (blob.contains(QLatin1String("librewolf"))) {
        gecko(QStringLiteral("LibreWolf"),
              {paths.configHome + QStringLiteral("/librewolf"), paths.home + QStringLiteral("/.librewolf")});
        return fp;
    }
    if (blob.contains(QLatin1String("floorp"))) {
        gecko(QStringLiteral("Floorp"), {paths.home + QStringLiteral("/.floorp"), paths.configHome + QStringLiteral("/floorp")});
        return fp;
    }
    if (blob.contains(QLatin1String("waterfox"))) {
        gecko(QStringLiteral("Waterfox"), {paths.home + QStringLiteral("/.waterfox")});
        return fp;
    }
    if (blob.contains(QLatin1String("firefox")) && !blob.contains(QLatin1String("pwa"))) {
        gecko(QStringLiteral("Firefox"),
              {paths.configHome + QStringLiteral("/mozilla/firefox"), paths.home + QStringLiteral("/.mozilla/firefox")});
        return fp;
    }
    if (blob.contains(QLatin1String("brave"))) {
        chromium(QStringLiteral("Brave"), paths.configHome + QStringLiteral("/BraveSoftware/Brave-Browser"));
        return fp;
    }
    if (blob.contains(QLatin1String("google-chrome")) || blob.contains(QLatin1String("google chrome"))) {
        chromium(QStringLiteral("Chrome"), paths.configHome + QStringLiteral("/google-chrome"));
        return fp;
    }
    if (blob.contains(QLatin1String("chromium"))) {
        chromium(QStringLiteral("Chromium"), paths.configHome + QStringLiteral("/chromium"));
        return fp;
    }
    if (blob.contains(QLatin1String("microsoft-edge")) || blob.contains(QLatin1String("msedge"))) {
        chromium(QStringLiteral("Edge"), paths.configHome + QStringLiteral("/microsoft-edge"));
        return fp;
    }
    if (blob.contains(QLatin1String("vivaldi"))) {
        chromium(QStringLiteral("Vivaldi"), paths.configHome + QStringLiteral("/vivaldi"));
        return fp;
    }
    if (blob.contains(QLatin1String("opera"))) {
        chromium(QStringLiteral("Opera"), paths.configHome + QStringLiteral("/opera"));
        return fp;
    }
    if (blob.contains(QLatin1String("thorium"))) {
        chromium(QStringLiteral("Thorium"), paths.configHome + QStringLiteral("/thorium"));
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

QList<Target> geckoProfiles(const DesktopApp &app, const Fingerprint &fp)
{
    QList<Target> out;
    const QString iniPath = fp.dataDir + QStringLiteral("/profiles.ini");
    if (!QFile::exists(iniPath)) {
        Target t;
        t.id = QStringLiteral("browser:") + app.id + QStringLiteral(":default");
        t.kind = Kind::BrowserProfile;
        t.engine = Engine::Gecko;
        t.name = QStringLiteral("Default");
        t.browserName = fp.brand.isEmpty() ? app.name : fp.brand;
        t.subtitle = t.browserName;
        t.exec = firstToken(app.execLine);
        t.args = {QStringLiteral("--new-tab"), QStringLiteral("$url")};
        t.icon = app.icon;
        t.isBrowserDefault = true;
        out.append(t);
        return out;
    }

    QSettings ini(iniPath, QSettings::IniFormat);
    QString defaultPath;
    const auto groups = ini.childGroups();
    for (const auto &g : groups) {
        if (g.startsWith(QLatin1String("Install"))) {
            ini.beginGroup(g);
            const QString def = ini.value(QStringLiteral("Default")).toString();
            ini.endGroup();
            if (!def.isEmpty()) {
                defaultPath = def;
            }
        }
    }

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
        if (r.path.isEmpty() || junkProfilePath(r.path)) {
            continue;
        }
        if (relative) {
            r.path = fp.dataDir + QLatin1Char('/') + r.path;
        }
        if (junkProfilePath(r.path)) {
            continue;
        }
        rows.append(r);
    }

    const QString exe = firstToken(app.execLine);
    for (const auto &r : rows) {
        Target t;
        const QString key = QFileInfo(r.path).fileName();
        t.id = QStringLiteral("browser:") + app.id + QLatin1Char(':') + key;
        t.kind = Kind::BrowserProfile;
        t.engine = Engine::Gecko;
        t.name = r.name.isEmpty() ? key : r.name;
        t.browserName = fp.brand.isEmpty() ? app.name : fp.brand;
        t.subtitle = t.browserName + QStringLiteral(" · ") + t.name;
        t.exec = exe;
        t.args = {QStringLiteral("-P"), r.name.isEmpty() ? key : r.name, QStringLiteral("--new-tab"), QStringLiteral("$url")};
        t.icon = app.icon;
        t.profileKey = r.name;
        t.profileDir = r.path;
        t.isBrowserDefault = r.isDefault || r.path.endsWith(defaultPath) || QFileInfo(r.path).fileName() == defaultPath;
        out.append(t);

        Target priv = t;
        priv.id += QStringLiteral(":private");
        priv.name = t.name + QStringLiteral(" (Private)");
        priv.subtitle = t.browserName + QStringLiteral(" · Private");
        priv.args = {QStringLiteral("-P"), r.name.isEmpty() ? key : r.name, QStringLiteral("--private-window"), QStringLiteral("$url")};
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
    const QString exe = firstToken(app.execLine);
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
        t.exec = exe;
        t.args = {QStringLiteral("--profile-directory=") + key, QStringLiteral("--new-tab"), QStringLiteral("$url")};
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
        inc.args = {QStringLiteral("--profile-directory=") + key, QStringLiteral("--incognito"), QStringLiteral("$url")};
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
        tor.exec = exe;
        tor.args = {QStringLiteral("--tor"), QStringLiteral("$url")};
        tor.icon = app.icon;
        tor.incognito = true;
        out.append(tor);
    }
    return out;
}

QList<Target> genericBrowser(const DesktopApp &app, const Fingerprint &fp)
{
    Target t;
    t.id = QStringLiteral("browser:") + app.id + QStringLiteral(":default");
    t.kind = Kind::BrowserProfile;
    t.engine = Engine::Generic;
    t.name = QStringLiteral("Default");
    t.browserName = fp.brand.isEmpty() ? app.name : fp.brand;
    t.subtitle = t.browserName;
    t.exec = firstToken(app.execLine);
    t.args = {QStringLiteral("$url")};
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
    copy.browserName = QStringLiteral("Tern");
    copy.subtitle = QStringLiteral("Clipboard");
    copy.icon = QStringLiteral("edit-copy");

    Target mail;
    mail.id = QStringLiteral("action:email");
    mail.kind = Kind::Action;
    mail.engine = Engine::Action;
    mail.name = QStringLiteral("Email link");
    mail.browserName = QStringLiteral("Tern");
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
    QSet<QString> seenExecBrand;
    for (const auto &app : apps) {
        const auto fp = fingerprint(app, paths);
        const QString sig = firstToken(app.execLine) + QLatin1Char('|') + fp.brand + QLatin1Char('|') + fp.dataDir;
        if (seenExecBrand.contains(sig)) {
            continue;
        }
        seenExecBrand.insert(sig);

        QList<Target> found;
        switch (fp.engine) {
        case Engine::Gecko:
            found = geckoProfiles(app, fp);
            break;
        case Engine::Chromium:
            found = chromiumProfiles(app, fp);
            break;
        default:
            found = genericBrowser(app, fp);
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
    for (auto &t : targets) {
        if (hidden.contains(t.id)) {
            t.hidden = true;
        }
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

} // namespace Tern
