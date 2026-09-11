#include "app/Controller.h"
#include "core/config.h"
#include "core/discovery.h"
#include "core/pipeline.h"
#include "core/router.h"
#include "tern_version.h"

#include <KAboutData>
#include <KDBusService>
#include <KCrash>
#include <KLocalizedString>

#include <QApplication>
#include <QCommandLineParser>
#include <QQuickStyle>

#include <cstdio>

static const char *actionName(Tern::Decision::Action action)
{
    switch (action) {
    case Tern::Decision::Action::Launch:
        return "launch";
    case Tern::Decision::Action::Pick:
        return "pick";
    case Tern::Decision::Action::Copy:
        return "copy";
    }
    return "pick";
}

static int explainUrl(const QString &url)
{
    const auto cfg = Tern::loadConfig(Tern::defaultConfigPath());
    const auto targets = Tern::applyConfigToTargets(Tern::discoverTargets(Tern::defaultDiscoveryPaths()), cfg);
    const Tern::Click click = Tern::runPipeline(url, cfg, {});
    const Tern::Decision d = Tern::route(click, targets, cfg);
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
    if (d.action == Tern::Decision::Action::Pick) {
        for (const auto &t : d.pickerTargets) {
            std::fprintf(stdout, "candidate\t%s\t%s\n", qPrintable(t.id), qPrintable(t.displayName()));
        }
    }
    return 0;
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("Tern"));
    app.setOrganizationName(QStringLiteral("tern"));
    app.setOrganizationDomain(QStringLiteral("tern.app"));
    app.setDesktopFileName(QStringLiteral("app.tern.Tern"));
    app.setApplicationVersion(QStringLiteral(TERN_VERSION_STRING));
    QQuickStyle::setStyle(QStringLiteral("org.kde.desktop"));
    KCrash::initialize();

    KLocalizedString::setApplicationDomain("tern");
    KAboutData about(QStringLiteral("tern"),
                     QStringLiteral("Tern"),
                     QStringLiteral(TERN_VERSION_STRING),
                     QStringLiteral("Open links in the right browser, profile, or app"),
                     KAboutLicense::Custom,
                     QStringLiteral("© 2026 Andy Hayes"));
    about.setLicenseText(QStringLiteral(
        "PolyForm Noncommercial License 1.0.0\n\n"
        "Personal and hobby use is free. Using Tern as part of a product "
        "you sell, or any other commercial use, needs a separate "
        "commercial license from Andy Hayes (andy@boundlessitsystems.com).\n\n"
        "See LICENSE and COMMERCIAL.md in the source repository for the "
        "full terms."));
    about.setOrganizationDomain(QByteArrayLiteral("tern.app"));
    about.setDesktopFileName(QStringLiteral("app.tern.Tern"));
    KAboutData::setApplicationData(about);
    app.setOrganizationDomain(QStringLiteral("tern.app"));
    app.setApplicationName(QStringLiteral("Tern"));
    app.setDesktopFileName(QStringLiteral("app.tern.Tern"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Tern — Plasma link router"));
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
        std::fprintf(stdout, "%s\n", qPrintable(Tern::defaultConfigPath()));
        return 0;
    }

    if (parser.isSet(listOpt)) {
        const auto cfg = Tern::loadConfig(Tern::defaultConfigPath());
        const auto targets = Tern::applyConfigToTargets(Tern::discoverTargets(Tern::defaultDiscoveryPaths()), cfg);
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
            std::fprintf(stderr, "tern --explain needs a URL\n");
            return 1;
        }
        return explainUrl(urls.first());
    }

    Tern::Controller controller;
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
