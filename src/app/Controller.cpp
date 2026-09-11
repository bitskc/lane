#include "Controller.h"

#include "Autostart.h"
#include "SourceInfo.h"
#include "core/config.h"
#include "core/destination.h"
#include "core/discovery.h"
#include "core/launcher.h"
#include "core/repoinfo.h"
#include "core/router.h"
#include "core/unshorten.h"
#include "core/urlutil.h"
#include "lane_version.h"

#include <LayerShellQt/Window>
#include <KCrash>
#include <KNotification>
#include <KStatusNotifierItem>
#include <KWaylandExtras>

#include <QClipboard>
#include <QDateTime>
#include <QDebug>
#include <QDesktopServices>
#include <QFileInfo>
#include <QGuiApplication>
#include <QLocale>
#include <QMenu>
#include <QProcess>
#include <QQmlContext>
#include <QQmlError>
#include <QQuickStyle>
#include <QScreen>
#include <QSharedPointer>
#include <QTimer>
#include <QUrl>
#include <QUuid>

namespace Lane
{

Controller::Controller(QObject *parent)
    : QObject(parent)
    , m_pickerModel(new PickerModel(this))
    , m_targetModel(new TargetModel(this))
    , m_ruleModel(new RuleModel(this))
    , m_updateChecker(new UpdateChecker(QStringLiteral(LANE_VERSION_STRING), this))
{
    m_configPath = defaultConfigPath();
    reload();
    m_tray = new KStatusNotifierItem(QStringLiteral("lane"), this);
    m_tray->setTitle(QStringLiteral("Lane"));
    m_tray->setToolTipTitle(QStringLiteral("Lane"));
    m_tray->setToolTipSubTitle(QStringLiteral("Link router"));
    m_tray->setIconByName(QStringLiteral("app.lane.Lane"));
    m_tray->setStatus(KStatusNotifierItem::Passive);
    m_tray->setCategory(KStatusNotifierItem::ApplicationStatus);
    m_tray->setStandardActionsEnabled(false);
    auto *menu = new QMenu();
    menu->addAction(QStringLiteral("Settings"), this, &Controller::openSettings);
    menu->addAction(QStringLiteral("Rediscover browsers"), this, &Controller::rediscover);
    m_tray->setContextMenu(menu);
    connect(m_tray, &KStatusNotifierItem::activateRequested, this, [this](bool, const QPoint &) {
        openSettings();
    });

    connect(m_updateChecker, &UpdateChecker::statusChanged, this, &Controller::updateStateChanged);

    connect(m_ruleModel, &RuleModel::rulesChanged, this, [this]() {
        m_config.rules = m_ruleModel->rules();
        persist();
    });
}

QString Controller::appVersion() const
{
    return QStringLiteral(LANE_VERSION_STRING);
}

QString Controller::projectUrl() const
{
    return githubProjectUrl();
}

QString Controller::updateCheckState() const
{
    switch (m_updateChecker->status()) {
    case UpdateChecker::Status::Checking:
        return QStringLiteral("checking");
    case UpdateChecker::Status::UpToDate:
        return QStringLiteral("up-to-date");
    case UpdateChecker::Status::UpdateAvailable:
        return QStringLiteral("update-available");
    case UpdateChecker::Status::Failed:
        return QStringLiteral("failed");
    case UpdateChecker::Status::Idle:
        break;
    }
    return QStringLiteral("idle");
}

QString Controller::updateLastChecked() const
{
    const QDateTime checkedAt = m_updateChecker->lastCheckedAt();
    if (!checkedAt.isValid()) {
        return QString();
    }
    return QStringLiteral("Last checked %1").arg(QLocale::system().toString(checkedAt, QLocale::ShortFormat));
}

void Controller::checkForUpdates()
{
    m_updateChecker->check();
}

void Controller::openExternalUrl(const QString &url)
{
    // Informational links in Lane's own Settings window (license, project
    // page, release notes) are app chrome, not a browsing click for Lane to
    // intercept, so this hands them to the desktop's own default-browser
    // resolution (QDesktopServices::openUrl) instead of calling openUrl()
    // directly.
    //
    // What that resolves to depends on who the default browser is:
    //   - Not Lane (this machine: Zen): the desktop opens Zen directly.
    //     Lane's pipeline and picker are never involved.
    //   - Lane itself: the OS hands the URL back to Lane (the same way it
    //     would for a link clicked in any other app), and Lane's normal
    //     openUrl() pipeline decides where it goes: a direct launch, a
    //     brief hold bar, or the picker if policy says to ask. That is
    //     intentional, not a bug: being the default browser means every
    //     https link, including this one, is subject to the same policy.
    //     It always terminates in a real target being launched or offered;
    //     Controller::launch() only ever spawns real discovered browsers,
    //     never Lane itself, so there is no loop back into this function.
    if (!isSafeOpenUrl(url) || isPrivateOrLocalHost(hostOf(url))) {
        return;
    }
    QDesktopServices::openUrl(QUrl(url));
}

QString Controller::currentPrettyUrl() const
{
    return displayUrl(m_click.openUrl);
}

bool Controller::currentSecure() const
{
    return parseUrl(m_click.openUrl).scheme.compare(QLatin1String("https"), Qt::CaseInsensitive) == 0
        && isSafeOpenUrl(m_click.openUrl);
}

void Controller::setAlwaysForHost(bool on)
{
    m_alwaysForHost = on;
    Q_EMIT currentChanged();
}

bool Controller::isDefaultBrowser() const
{
    QProcess p;
    p.start(QStringLiteral("xdg-settings"), {QStringLiteral("get"), QStringLiteral("default-web-browser")});
    p.waitForFinished(1500);
    return QString::fromUtf8(p.readAllStandardOutput()).trimmed() == QLatin1String("app.lane.Lane.desktop");
}

QString Controller::pickerPolicy() const
{
    return pickerPolicyToString(m_config.pickerPolicy);
}

void Controller::setPickerPolicy(const QString &p)
{
    m_config.pickerPolicy = pickerPolicyFromString(p);
    persist();
    Q_EMIT settingsChanged();
}

void Controller::setPreferPwa(bool on)
{
    m_config.preferPwa = on;
    persist();
    Q_EMIT settingsChanged();
}

void Controller::setToastEnabled(bool on)
{
    m_config.toast = on;
    persist();
    Q_EMIT settingsChanged();
}

void Controller::setUnwrapO365(bool on)
{
    m_config.unwrapO365 = on;
    persist();
    Q_EMIT settingsChanged();
}

void Controller::setUnshorten(bool on)
{
    m_config.unshorten = on;
    persist();
    Q_EMIT settingsChanged();
}

void Controller::setAutostartEnabled(bool on)
{
    m_config.autostart = on;
    setAutostart(on);
    persist();
    Q_EMIT settingsChanged();
}

void Controller::setCloseOnFocusLoss(bool on)
{
    m_config.closeOnFocusLoss = on;
    persist();
    Q_EMIT settingsChanged();
}

void Controller::setShowUrl(bool on)
{
    m_config.showUrl = on;
    persist();
    Q_EMIT settingsChanged();
}

void Controller::setDefaultTargetId(const QString &id)
{
    m_config.defaultTargetId = id;
    persist();
    Q_EMIT settingsChanged();
}

QStringList Controller::targetIds() const
{
    QStringList ids;
    for (const auto &t : m_targets) {
        if (!t.hidden) {
            ids << t.id;
        }
    }
    return ids;
}

QStringList Controller::targetNames() const
{
    QStringList names;
    for (const auto &t : m_targets) {
        if (!t.hidden) {
            names << t.displayName();
        }
    }
    return names;
}

QStringList Controller::rememberedHosts() const
{
    return m_config.remembered.keys();
}

void Controller::setHoldAutoOpen(bool on)
{
    m_config.holdAutoOpen = on;
    persist();
    Q_EMIT settingsChanged();
}

void Controller::setHoldMs(int ms)
{
    ms = qBound(200, ms, 10000);
    if (ms == m_config.holdMs) {
        return;
    }
    m_config.holdMs = ms;
    persist();
    Q_EMIT settingsChanged();
}

void Controller::setDestinationIndex(int idx)
{
    idx = qBound(0, idx, qMax(0, m_destinationLadder.size() - 1));
    if (idx == m_destinationIndex) {
        return;
    }
    m_destinationIndex = idx;
    Q_EMIT currentChanged();
}

QString Controller::currentDestinationKey() const
{
    if (m_destinationLadder.isEmpty()) {
        return m_click.host;
    }
    return m_destinationLadder.value(m_destinationIndex, m_destinationLadder.constFirst());
}

void Controller::handleArgs(const QStringList &args)
{
    QStringList rest = args;
    if (!rest.isEmpty() && rest.first().contains(QLatin1String("lane"))) {
        rest.removeFirst();
    }
    bool daemon = false;
    bool forcePicker = false;
    QString url;
    for (const QString &a : rest) {
        if (a == QLatin1String("--daemon")) {
            daemon = true;
        } else if (a == QLatin1String("--pick") || a == QLatin1String("-p")) {
            forcePicker = true;
        } else if (a == QLatin1String("--settings") || a == QLatin1String("--configure")) {
            openSettings();
        } else if (a == QLatin1String("--rediscover")) {
            rediscover();
        } else if (!a.startsWith(QLatin1Char('-'))) {
            url = a;
        }
    }
    if (!url.isEmpty()) {
        openUrl(url, forcePicker);
        return;
    }
    if (!daemon && rest.contains(QStringLiteral("--settings")) == false && rest.isEmpty()) {
        openSettings();
    }
}

void Controller::openUrl(const QString &url, bool forcePicker)
{
    // KDBusService sets XDG_ACTIVATION_TOKEN in Lane's own environment for
    // the duration of the activateRequested/openRequested signal when the
    // click that invoked Lane carried one, and unsets it again right after
    // (see KDBusService::activateRequested docs). The first invocation
    // (no daemon running yet) can also inherit one directly at process
    // start. Either way, capture and consume it now so a later click that
    // carries no token of its own can never reuse a stale one.
    m_pendingActivationToken = qEnvironmentVariable("XDG_ACTIVATION_TOKEN");
    if (!m_pendingActivationToken.isEmpty()) {
        qunsetenv("XDG_ACTIVATION_TOKEN");
    }

    if (m_holdAnimation && m_holdAnimation->state() == QAbstractAnimation::Running) {
        m_holdAnimation->stop();
        hideHold();
    }

    const auto src = activeSource();
    m_click = runPipeline(url, m_config, unshortenFn());
    m_click.forcePicker = forcePicker;
    m_click.processName = src.processName;
    m_click.windowTitle = src.windowTitle;
    m_alwaysForHost = false;

    const Decision d = route(m_click, m_targets, m_config);

    m_destinationLadder = Lane::destinationLadder(m_click.matchUrl);
    const Target *suggested = d.action == Decision::Action::Launch ? &d.target : nullptr;
    m_destinationIndex = Lane::suggestedLadderIndex(m_click.matchUrl, suggested, m_config.remembered);

    Q_EMIT currentChanged();
    applyDecision(d);
}

void Controller::pick(int row)
{
    const Target t = m_pickerModel->targetAt(row);
    if (t.id.isEmpty()) {
        return;
    }
    pickId(t.id);
}

void Controller::pickId(const QString &id)
{
    const Target *t = findTarget(m_targets, id);
    if (!t) {
        return;
    }
    if (m_alwaysForHost && !m_click.host.isEmpty() && t->kind != Kind::Action
        && isSafeOpenUrl(m_click.openUrl) && !isPrivateOrLocalHost(m_click.host)) {
        m_config.remembered.insert(currentDestinationKey(), t->id);
        persist();
    }
    if (t->id == QLatin1String("action:copy")) {
        hidePicker();
        copyCurrent();
        return;
    }
    // Request the activation token while the picker window still holds
    // focus (below), then dismiss it: dismissing releases the layer-shell
    // exclusive keyboard grab, which must happen before the launched
    // target's window can take focus, but requesting the token needs the
    // window's still-fresh input event first.
    QWindow *window = (m_pickerWindow && m_pickerWindow->isVisible()) ? m_pickerWindow.data() : nullptr;
    requestActivationAndLaunch(*t, QStringLiteral("picker"), window);
    hidePicker();
}

void Controller::cancelPicker()
{
    hidePicker();
}

void Controller::copyCurrent()
{
    if (auto *clip = QGuiApplication::clipboard()) {
        const QString safe = sanitizedOpenUrl(m_click.openUrl);
        clip->setText(safe.isEmpty() ? displayUrl(m_click.openUrl) : safe);
    }
    hidePicker();
}

void Controller::openSettings()
{
    ensureSettingsEngine();
    if (m_settingsWindow) {
        m_settingsWindow->show();
        m_settingsWindow->requestActivate();
        m_settingsWindow->raise();
    }
}

void Controller::rediscover()
{
    reload();
}

void Controller::makeDefaultBrowser()
{
    QProcess::execute(QStringLiteral("xdg-mime"),
                      {QStringLiteral("default"), QStringLiteral("app.lane.Lane.desktop"), QStringLiteral("x-scheme-handler/http")});
    QProcess::execute(QStringLiteral("xdg-mime"),
                      {QStringLiteral("default"), QStringLiteral("app.lane.Lane.desktop"), QStringLiteral("x-scheme-handler/https")});
    QProcess::execute(QStringLiteral("xdg-settings"),
                      {QStringLiteral("set"), QStringLiteral("default-web-browser"), QStringLiteral("app.lane.Lane.desktop")});
    Q_EMIT defaultBrowserChanged();
}

void Controller::hideTarget(const QString &id, bool hidden)
{
    m_config.hiddenTargetIds.removeAll(id);
    if (hidden) {
        m_config.hiddenTargetIds.append(id);
    }
    persist();
    m_targets = applyConfigToTargets(discoverTargets(defaultDiscoveryPaths()), m_config);
    m_targetModel->setTargets(m_targets);
    Q_EMIT settingsChanged();
}

void Controller::save()
{
    persist();
}

QString Controller::displayNameFor(const QString &id) const
{
    if (const Target *t = findTarget(m_targets, id)) {
        return t->displayName();
    }
    return id;
}

QString Controller::rememberedTarget(const QString &host) const
{
    return m_config.remembered.value(host);
}

void Controller::forgetHost(const QString &host)
{
    m_config.remembered.remove(host);
    persist();
    Q_EMIT settingsChanged();
}

bool Controller::targetExists(const QString &id) const
{
    return findTarget(m_targets, id) != nullptr;
}

QStringList Controller::danglingRememberedHosts() const
{
    return danglingRememberedKeys(m_targets, m_config);
}

void Controller::clearDeadRemembered()
{
    const QStringList dead = danglingRememberedKeys(m_targets, m_config);
    if (dead.isEmpty()) {
        return;
    }
    for (const auto &host : dead) {
        m_config.remembered.remove(host);
    }
    persist();
    Q_EMIT settingsChanged();
}

QString Controller::addCustomTarget(const QString &name, const QString &command)
{
    Target t;
    QString error;
    if (!parseCustomCommand(command, &t, &error)) {
        qWarning() << "Lane: rejected custom handler:" << error;
        return error;
    }
    t.name = name.trimmed().isEmpty() ? QFileInfo(t.exec).fileName() : name.trimmed();
    t.browserName = QStringLiteral("Custom");
    t.subtitle = QStringLiteral("Custom");
    t.id = QStringLiteral("custom:") + QUuid::createUuid().toString(QUuid::WithoutBraces);
    t.icon = QStringLiteral("application-x-executable");
    m_config.customTargets.append(t);
    persist();
    m_targets = applyConfigToTargets(discoverTargets(defaultDiscoveryPaths()), m_config);
    m_targetModel->setTargets(m_targets);
    Q_EMIT settingsChanged();
    return {};
}

void Controller::removeCustomTarget(const QString &id)
{
    QList<Target> kept;
    for (const auto &t : m_config.customTargets) {
        if (t.id != id) {
            kept.append(t);
        }
    }
    m_config.customTargets = kept;
    persist();
    m_targets = applyConfigToTargets(discoverTargets(defaultDiscoveryPaths()), m_config);
    m_targetModel->setTargets(m_targets);
    Q_EMIT settingsChanged();
}

void Controller::renameTarget(const QString &id, const QString &name)
{
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty()) {
        m_config.targetAliases.remove(id);
    } else {
        m_config.targetAliases.insert(id, trimmed);
    }
    for (auto &t : m_config.customTargets) {
        if (t.id == id) {
            t.name = trimmed.isEmpty() ? QFileInfo(t.exec).fileName() : trimmed;
            break;
        }
    }
    persist();
    m_targets = applyConfigToTargets(discoverTargets(defaultDiscoveryPaths()), m_config);
    m_targetModel->setTargets(m_targets);
    Q_EMIT settingsChanged();
}

void Controller::moveTarget(const QString &id, int newIndexInKind)
{
    m_config.targetOrder = moveIdAmongSiblings(m_targets, id, newIndexInKind);
    persist();
    m_targets = applyConfigToTargets(discoverTargets(defaultDiscoveryPaths()), m_config);
    m_targetModel->setTargets(m_targets);
    Q_EMIT settingsChanged();
}

void Controller::reload()
{
    m_config = loadConfig(m_configPath);
    m_config.autostart = autostartEnabled();
    m_targets = applyConfigToTargets(discoverTargets(defaultDiscoveryPaths()), m_config);
    if (m_config.defaultTargetId.isEmpty()) {
        if (const Target *t = defaultTarget(m_targets, m_config)) {
            m_config.defaultTargetId = t->id;
        }
    }
    m_targetModel->setTargets(m_targets);
    m_ruleModel->setRules(m_config.rules);
    Q_EMIT settingsChanged();
    Q_EMIT defaultBrowserChanged();
}

void Controller::persist()
{
    m_config.rules = m_ruleModel->rules();
    saveConfig(m_configPath, m_config);
}

void Controller::applyDecision(const Decision &d)
{
    if (d.action == Decision::Action::Pick) {
        if (d.reason == QLatin1String("blocked")) {
            // Every row in the picker would fail the same isSafeOpenUrl()
            // check the moment it was launched (see launch() below), so a
            // picker here would be a list of destinations that all silently
            // fail. Say why up front instead.
            notifyBlocked();
            return;
        }
        m_pickerModel->reset(d.pickerTargets);
        showPicker();
        return;
    }
    if (d.target.id == QLatin1String("action:copy")) {
        copyCurrent();
        return;
    }
    if (shouldHold(d.reason)) {
        startHold(d.target, d.reason, d.memoryKey);
        return;
    }
    launch(d.target, d.reason, m_pendingActivationToken);
}

void Controller::showPicker()
{
    ensurePickerEngine();
    if (m_pickerWindow) {
        m_pickerWindow->show();
        m_pickerWindow->requestActivate();
    }
    Q_EMIT pickerVisibleChanged(true);
}

void Controller::hidePicker()
{
    if (m_pickerWindow) {
        m_pickerWindow->hide();
    }
    Q_EMIT pickerVisibleChanged(false);
}

void Controller::launch(const Target &target, const QString &reason, const QString &activationToken)
{
    if (target.id.isEmpty()) {
        showPicker();
        return;
    }
    if (!isSafeOpenUrl(m_click.openUrl)) {
        notifyBlocked();
        return;
    }
    if (!launchTarget(target, m_click.openUrl, activationToken)) {
        auto *n = new KNotification(QStringLiteral("launch-failed"), KNotification::CloseOnTimeout, this);
        n->setComponentName(QStringLiteral("app.lane.Lane"));
        n->setTitle(QStringLiteral("Could not open in %1").arg(target.displayName()));
        n->setText(m_click.host.isEmpty() ? QStringLiteral("The launch failed.") : m_click.host);
        n->setIconName(QStringLiteral("dialog-error"));
        n->sendEvent();
        return;
    }
    m_config.recentTargetIds.removeAll(target.id);
    m_config.recentTargetIds.prepend(target.id);
    while (m_config.recentTargetIds.size() > 12) {
        m_config.recentTargetIds.removeLast();
    }
    persist();
    if (m_config.toast) {
        toast(target, reason);
    }
}

void Controller::requestActivationAndLaunch(const Target &target, const QString &reason, QWindow *window)
{
    if (!window) {
        // No Lane-owned surface was involved (a silent rule/remembered/
        // default launch with no overlay ever shown): the best available
        // token is whatever this click's openUrl() call already received
        // from whoever invoked Lane, if anything.
        launch(target, reason, m_pendingActivationToken);
        return;
    }
    auto resolved = QSharedPointer<bool>::create(false);
    auto finish = [this, target, reason, resolved](const QString &token) {
        if (*resolved) {
            // Either the compositor already answered and the fallback
            // timer fired anyway, or vice versa; only the first launches.
            return;
        }
        *resolved = true;
        launch(target, reason, token);
    };
    KWaylandExtras::xdgActivationToken(window, QString()).then(this, finish);
    // Guard against a compositor that never answers (no xdg-activation
    // support, or a stalled request): the launch must never wait on a
    // token forever. A real Wayland round-trip is sub-millisecond; 300ms
    // is generous headroom before falling back to an unraised launch.
    QTimer::singleShot(300, this, [finish]() { finish(QString()); });
}

void Controller::toast(const Target &target, const QString &reason)
{
    auto *n = new KNotification(QStringLiteral("opened"), KNotification::CloseOnTimeout, this);
    n->setComponentName(QStringLiteral("app.lane.Lane"));
    n->setTitle(QStringLiteral("Opened in %1").arg(target.displayName()));
    n->setText(m_click.host.isEmpty() ? QStringLiteral("Link opened") : m_click.host);
    n->setIconName(target.icon.isEmpty() ? QStringLiteral("app.lane.Lane") : target.icon);
    Q_UNUSED(reason);
    n->sendEvent();
}

void Controller::notifyBlocked()
{
    auto *n = new KNotification(QStringLiteral("opened"), KNotification::CloseOnTimeout, this);
    n->setComponentName(QStringLiteral("app.lane.Lane"));
    n->setTitle(QStringLiteral("Lane blocked this link"));
    n->setText(QStringLiteral("Only http and https links can be opened."));
    n->setIconName(QStringLiteral("security-high"));
    n->sendEvent();
}

void Controller::ensurePickerEngine()
{
    if (m_pickerEngine) {
        return;
    }
    m_pickerEngine = new QQmlApplicationEngine(this);
    connect(m_pickerEngine, &QQmlApplicationEngine::warnings, this, [](const QList<QQmlError> &warnings) {
        for (const auto &w : warnings) {
            qWarning() << "Lane picker:" << w.toString();
        }
    });
    m_pickerEngine->rootContext()->setContextProperty(QStringLiteral("controller"), this);
    m_pickerEngine->loadFromModule(QStringLiteral("app.lane"), QStringLiteral("Picker"));
    if (m_pickerEngine->rootObjects().isEmpty()) {
        qWarning() << "Lane: picker QML failed to load";
        return;
    }
    m_pickerWindow = qobject_cast<QWindow *>(m_pickerEngine->rootObjects().constFirst());
    if (m_pickerWindow) {
        configureLayerShell(m_pickerWindow);
    }
}

void Controller::ensureSettingsEngine()
{
    if (m_settingsEngine) {
        return;
    }
    m_settingsEngine = new QQmlApplicationEngine(this);
    connect(m_settingsEngine, &QQmlApplicationEngine::warnings, this, [](const QList<QQmlError> &warnings) {
        for (const auto &w : warnings) {
            qWarning() << "Lane settings:" << w.toString();
        }
    });
    m_settingsEngine->rootContext()->setContextProperty(QStringLiteral("controller"), this);
    m_settingsEngine->loadFromModule(QStringLiteral("app.lane"), QStringLiteral("Settings"));
    if (m_settingsEngine->rootObjects().isEmpty()) {
        qWarning() << "Lane: settings QML failed to load";
        return;
    }
    m_settingsWindow = qobject_cast<QWindow *>(m_settingsEngine->rootObjects().constFirst());
}

void Controller::configureLayerShell(QWindow *window, const QString &scope)
{
    auto *ls = LayerShellQt::Window::get(window);
    ls->setLayer(LayerShellQt::Window::LayerOverlay);
    LayerShellQt::Window::Anchors anchors;
    anchors.setFlag(LayerShellQt::Window::AnchorTop);
    anchors.setFlag(LayerShellQt::Window::AnchorBottom);
    anchors.setFlag(LayerShellQt::Window::AnchorLeft);
    anchors.setFlag(LayerShellQt::Window::AnchorRight);
    ls->setAnchors(anchors);
    ls->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityExclusive);
    ls->setExclusiveZone(-1);
    ls->setScope(scope);
    ls->setWantsToBeOnActiveScreen(true);
    ls->setActivateOnShow(true);
    auto *screen = window->screen() ? window->screen() : QGuiApplication::primaryScreen();
    if (screen) {
        window->setGeometry(screen->geometry());
    }
}

UnshortenFn Controller::unshortenFn() const
{
    if (!m_config.unshorten) {
        return {};
    }
    return [](const QString &url) { return unshortenSync(url, 1800); };
}

bool Controller::shouldHold(const QString &reason) const
{
    if (!m_config.holdAutoOpen || m_config.holdMs <= 0) {
        return false;
    }
    return reason == QLatin1String("remembered")
        || reason == QLatin1String("pwa")
        || reason == QLatin1String("default");
}

void Controller::startHold(const Target &target, const QString &reason, const QString &memoryKey)
{
    m_holdTarget = target;
    m_holdReason = reason;
    m_holdMemoryKey = memoryKey;
    m_holdTargetName = target.displayName();
    m_holdDestinationKey = memoryKey.isEmpty() ? displayUrl(m_click.openUrl) : memoryKey;
    m_holdProgress = 0;

    Q_EMIT holdChanged();
    Q_EMIT holdProgressChanged();

    ensureHoldEngine();
    if (m_holdWindow) {
        m_holdWindow->show();
        m_holdWindow->requestActivate();
    }

    if (!m_holdAnimation) {
        m_holdAnimation = new QVariantAnimation(this);
        connect(m_holdAnimation, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
            m_holdProgress = v.toReal();
            Q_EMIT holdProgressChanged();
        });
        connect(m_holdAnimation, &QVariantAnimation::finished, this, [this]() {
            QWindow *window = (m_holdWindow && m_holdWindow->isVisible()) ? m_holdWindow.data() : nullptr;
            requestActivationAndLaunch(m_holdTarget, m_holdReason, window);
            hideHold();
        });
    }
    m_holdAnimation->setDuration(qMax(1, m_config.holdMs));
    m_holdAnimation->setStartValue(0.0);
    m_holdAnimation->setEndValue(1.0);
    m_holdAnimation->start();
}

void Controller::confirmHold()
{
    if (!m_holdAnimation || m_holdAnimation->state() != QAbstractAnimation::Running) {
        return;
    }
    m_holdAnimation->stop();
    QWindow *window = (m_holdWindow && m_holdWindow->isVisible()) ? m_holdWindow.data() : nullptr;
    requestActivationAndLaunch(m_holdTarget, m_holdReason, window);
    hideHold();
}

void Controller::cancelHold()
{
    if (!m_holdAnimation || m_holdAnimation->state() != QAbstractAnimation::Running) {
        return;
    }
    m_holdAnimation->stop();
    hideHold();
    m_pickerModel->reset(rankForPicker(m_click, m_targets, m_config));
    showPicker();
}

void Controller::hideHold()
{
    if (m_holdWindow) {
        m_holdWindow->hide();
    }
}

void Controller::ensureHoldEngine()
{
    if (m_holdEngine) {
        return;
    }
    m_holdEngine = new QQmlApplicationEngine(this);
    connect(m_holdEngine, &QQmlApplicationEngine::warnings, this, [](const QList<QQmlError> &warnings) {
        for (const auto &w : warnings) {
            qWarning() << "Lane hold:" << w.toString();
        }
    });
    m_holdEngine->rootContext()->setContextProperty(QStringLiteral("controller"), this);
    m_holdEngine->loadFromModule(QStringLiteral("app.lane"), QStringLiteral("Hold"));
    if (m_holdEngine->rootObjects().isEmpty()) {
        qWarning() << "Lane: hold QML failed to load";
        return;
    }
    m_holdWindow = qobject_cast<QWindow *>(m_holdEngine->rootObjects().constFirst());
    if (m_holdWindow) {
        configureLayerShell(m_holdWindow, QStringLiteral("lane-hold"));
    }
}

} // namespace Lane
