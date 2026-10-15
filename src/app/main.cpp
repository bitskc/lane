#include "app/Controller.h"
#include "app/CrashLog.h"
#include "app/platform/platform.h"
#include "app/SingleInstance.h"
#ifdef Q_OS_MACOS
#include "app/UrlEventFilter.h"
#endif
#include "core/config.h"
#include "core/discovery.h"
#include "core/pipeline.h"
#include "core/router.h"
#include "lane_version.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>

#include <cstdio>

static const char *actionName(Lane::Decision::Action action)
{
    switch (action) {
    case Lane::Decision::Action::Launch:
        return "launch";
    case Lane::Decision::Action::Pick:
        return "pick";
    case Lane::Decision::Action::Copy:
        return "copy";
    }
    return "pick";
}

static int explainUrl(const QString &url)
{
    const auto cfg = Lane::loadConfig(Lane::defaultConfigPath());
    const auto targets = Lane::applyConfigToTargets(Lane::discoverTargets(Lane::defaultDiscoveryPaths()), cfg);
    const Lane::Click click = Lane::runPipeline(url, cfg, {});
    const Lane::Decision d = Lane::route(click, targets, cfg);
    std::fprintf(stdout, "url\t%s\n", qPrintable(click.originalUrl));
    std::fprintf(stdout, "match\t%s\n", qPrintable(click.matchUrl));
    if (!click.removedTrackingParams.isEmpty()) {
        std::fprintf(stdout, "cleaned\t%s\n", qPrintable(click.removedTrackingParams.join(QStringLiteral(", "))));
    }
    std::fprintf(stdout, "host\t%s\n", qPrintable(click.host));
    std::fprintf(stdout, "action\t%s\n", actionName(d.action));
    std::fprintf(stdout, "reason\t%s\n", qPrintable(d.reason));
    if (!d.memoryKey.isEmpty()) {
        std::fprintf(stdout, "memoryKey\t%s\n", qPrintable(d.memoryKey));
    }
    if (!d.target.id.isEmpty()) {
        std::fprintf(stdout, "target\t%s\t%s\n", qPrintable(d.target.id), qPrintable(d.target.displayName()));
    }
    if (d.action == Lane::Decision::Action::Pick) {
        for (const auto &t : d.pickerTargets) {
            std::fprintf(stdout, "candidate\t%s\t%s\n", qPrintable(t.id), qPrintable(t.displayName()));
        }
    }
    return 0;
}

int main(int argc, char **argv)
{
    // Plasma's platformtheme plugin calls QQuickStyle::setStyle("org.kde.desktop")
    // from inside QApplication's init, which drags Kirigami and
    // qqc2-desktop-style into the process. The env var outranks that
    // hook, so it is set here, before QGuiApplication constructs the
    // platform theme. The qtquickcontrols2.conf resource covers hosts
    // that do not run a KDE platformtheme plugin.
    qputenv("QT_QUICK_CONTROLS_STYLE", "Fusion");

    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);
    Lane::migrateLegacyConfig();
    app.setApplicationName(QStringLiteral("Lane"));
    app.setOrganizationName(QStringLiteral("lane"));
    app.setOrganizationDomain(QStringLiteral("lane.app"));
    app.setDesktopFileName(QStringLiteral("app.lane.Lane"));
    app.setApplicationVersion(QStringLiteral(LANE_VERSION_STRING));
    Lane::CrashLog::install();

#ifdef Q_OS_MACOS
    // LaunchServices posts QFileOpenEvent to the application object from
    // the first click, including events raised before Controller exists;
    // the filter queues them and drains once openUrl is wired below.
    Lane::UrlEventFilter urlEvents(&app);
    app.installEventFilter(&urlEvents);
#endif

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Lane — Plasma link router"));
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption daemonOpt(QStringLiteral("daemon"), QStringLiteral("Run in the background without opening settings"));
    QCommandLineOption pickOpt(QStringList{QStringLiteral("p"), QStringLiteral("pick")}, QStringLiteral("Always show the picker"));
    QCommandLineOption listOpt(QStringLiteral("list"), QStringLiteral("Print discovered targets and exit"));
    QCommandLineOption explainOpt(QStringLiteral("explain"), QStringLiteral("Print the routing decision and exit"));
    QCommandLineOption settingsOpt(QStringList{QStringLiteral("settings"), QStringLiteral("configure")}, QStringLiteral("Open settings"));
    QCommandLineOption rediscoverOpt(QStringLiteral("rediscover"), QStringLiteral("Reload config and rescan browser targets in the running daemon"));
    QCommandLineOption configPathOpt(QStringLiteral("config-path"), QStringLiteral("Print the config file path and exit"));
    parser.addOption(daemonOpt);
    parser.addOption(pickOpt);
    parser.addOption(listOpt);
    parser.addOption(explainOpt);
    parser.addOption(settingsOpt);
    parser.addOption(rediscoverOpt);
    parser.addOption(configPathOpt);
    parser.addPositionalArgument(QStringLiteral("url"), QStringLiteral("URL to open"), QStringLiteral("[url]"));
    parser.process(app);

    if (parser.isSet(configPathOpt)) {
        std::fprintf(stdout, "%s\n", qPrintable(Lane::defaultConfigPath()));
        return 0;
    }

    if (parser.isSet(listOpt)) {
        const auto cfg = Lane::loadConfig(Lane::defaultConfigPath());
        const auto targets = Lane::applyConfigToTargets(Lane::discoverTargets(Lane::defaultDiscoveryPaths()), cfg);
        std::fprintf(stdout, "targets: %lld\n", static_cast<long long>(targets.size()));
        for (const auto &t : targets) {
            if (t.hidden) {
                continue;
            }
            std::fprintf(stdout,
                         "%s\t%s\t%s\t%s\n",
                         qPrintable(t.id),
                         qPrintable(t.displayName()),
                         qPrintable(t.exec),
                         qPrintable(kindName(t.kind)));
        }
        return 0;
    }

    if (parser.isSet(explainOpt)) {
        const auto urls = parser.positionalArguments();
        if (urls.isEmpty()) {
            std::fprintf(stderr, "lane --explain needs a URL\n");
            return 1;
        }
        return explainUrl(urls.first());
    }

    // Unique-instance must be claimed before Controller does its startup
    // work (discovery scan + bounded default-handler queries):
    // a duplicate `lane <url>` invocation that reaches past this line is
    // about to exit once the socket refuses a second listener, so
    // anything spent building a Controller here is wasted.
    Lane::SingleInstance instance;
    if (!instance.claim()) {
        return 0;
    }

    Lane::Controller controller;

    instance.onMessage = [&controller](const QJsonObject &payload) {
        const QString token = payload.value(QStringLiteral("activationToken")).toString();
        if (!token.isEmpty()) {
            qputenv("XDG_ACTIVATION_TOKEN", token.toUtf8());
        }
        QStringList args;
        for (const QJsonValue &v : payload.value(QStringLiteral("args")).toArray()) {
            args << v.toString();
        }
        controller.handleArgs(args);
        if (!token.isEmpty()) {
            qunsetenv("XDG_ACTIVATION_TOKEN");
        }
    };

    // Bus-name acquisition is the systemd Type=dbus readiness signal and
    // the only way a launcher's gtk-launch/Open call can reach a running
    // daemon; a failure here is as fatal as KDBusService::Unique's used
    // to be. On macOS this always succeeds (LaunchServices needs none).
    if (!Lane::platform()->registerActivation(
            [&controller](const QStringList &args) { controller.handleArgs(args); },
            [&controller](const QString &url) { controller.openUrl(url); })) {
        std::fprintf(stderr, "lane: could not register D-Bus activation; exiting\n");
        return 1;
    }

    controller.handleArgs(QCoreApplication::arguments());

#ifdef Q_OS_MACOS
    // Any URL events queued during startup dispatch here, in order,
    // after positional argv is handled so an explicit URL wins first.
    urlEvents.setSink([&controller](const QString &url) { controller.openUrl(url); });
#endif

    return app.exec();
}
