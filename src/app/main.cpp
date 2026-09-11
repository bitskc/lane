#include "app/Controller.h"
#include "core/config.h"
#include "core/discovery.h"
#include "core/pipeline.h"
#include "core/router.h"
#include "lane_version.h"

#include <KAboutData>
#include <KDBusService>
#include <KCrash>
#include <KLocalizedString>

#include <QApplication>
#include <QCommandLineParser>
#include <QQuickStyle>

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
    QApplication app(argc, argv);
    Lane::migrateLegacyConfig();
    app.setApplicationName(QStringLiteral("Lane"));
    app.setOrganizationName(QStringLiteral("lane"));
    app.setOrganizationDomain(QStringLiteral("lane.app"));
    app.setDesktopFileName(QStringLiteral("app.lane.Lane"));
    app.setApplicationVersion(QStringLiteral(LANE_VERSION_STRING));
    QQuickStyle::setStyle(QStringLiteral("org.kde.desktop"));
    KCrash::initialize();

    KLocalizedString::setApplicationDomain("lane");
    KAboutData about(QStringLiteral("lane"),
                     QStringLiteral("Lane"),
                     QStringLiteral(LANE_VERSION_STRING),
                     QStringLiteral("Open links in the right browser, profile, or app"),
                     KAboutLicense::Custom,
                     QStringLiteral("© 2026 Andy Hayes"));
    about.setLicenseText(QStringLiteral(
        "PolyForm Noncommercial License 1.0.0\n\n"
        "Personal and hobby use is free. Using Lane as part of a product "
        "you sell, or any other commercial use, needs a separate "
        "commercial license from Andy Hayes (andy@boundlessitsystems.com).\n\n"
        "See LICENSE and COMMERCIAL.md in the source repository for the "
        "full terms."));
    about.setOrganizationDomain(QByteArrayLiteral("lane.app"));
    about.setDesktopFileName(QStringLiteral("app.lane.Lane"));
    KAboutData::setApplicationData(about);
    app.setOrganizationDomain(QStringLiteral("lane.app"));
    app.setApplicationName(QStringLiteral("Lane"));
    app.setDesktopFileName(QStringLiteral("app.lane.Lane"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Lane — Plasma link router"));
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption daemonOpt(QStringLiteral("daemon"), QStringLiteral("Run in the background without opening settings"));
    QCommandLineOption pickOpt(QStringList{QStringLiteral("p"), QStringLiteral("pick")}, QStringLiteral("Always show the picker"));
    QCommandLineOption listOpt(QStringLiteral("list"), QStringLiteral("Print discovered targets and exit"));
    QCommandLineOption explainOpt(QStringLiteral("explain"), QStringLiteral("Print the routing decision and exit"));
    QCommandLineOption settingsOpt(QStringLiteral("settings"), QStringLiteral("Open settings"));
    QCommandLineOption configPathOpt(QStringLiteral("config-path"), QStringLiteral("Print the config file path and exit"));
    parser.addOption(daemonOpt);
    parser.addOption(pickOpt);
    parser.addOption(listOpt);
    parser.addOption(explainOpt);
    parser.addOption(settingsOpt);
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
                         "%s\t%s\t%s\n",
                         qPrintable(t.id),
                         qPrintable(t.displayName()),
                         qPrintable(t.exec));
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

    Lane::Controller controller;
    KDBusService service(KDBusService::Unique);
    QObject::connect(&service, &KDBusService::activateRequested, &controller, [&](const QStringList &args, const QString &) {
        controller.handleArgs(args);
    });
    QObject::connect(&service, &KDBusService::openRequested, &controller, [&](const QList<QUrl> &urls) {
        for (const auto &u : urls) {
            controller.openUrl(u.toString());
        }
    });

    controller.handleArgs(QCoreApplication::arguments());

    return app.exec();
}
