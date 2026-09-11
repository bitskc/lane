#include "Controller.h"

#include "Autostart.h"
#include "SourceInfo.h"
#include "core/config.h"
#include "core/destination.h"
#include "core/discovery.h"
#include "core/launcher.h"
#include "core/router.h"
#include "core/unshorten.h"
#include "core/urlutil.h"
#include "tern_version.h"

#include <LayerShellQt/Window>
#include <KCrash>
#include <KNotification>
#include <KStatusNotifierItem>

#include <QClipboard>
#include <QDebug>
#include <QFileInfo>
#include <QGuiApplication>
#include <QMenu>
#include <QProcess>
#include <QQmlContext>
#include <QQmlError>
#include <QQuickStyle>
#include <QScreen>
#include <QTimer>
#include <QUuid>

namespace Tern
{

Controller::Controller(QObject *parent)
    : QObject(parent)
    , m_pickerModel(new PickerModel(this))
    , m_targetModel(new TargetModel(this))
    , m_ruleModel(new RuleModel(this))
{
    m_configPath = defaultConfigPath();
    reload();
    m_tray = new KStatusNotifierItem(QStringLiteral("tern"), this);
    m_tray->setTitle(QStringLiteral("Tern"));
    m_tray->setToolTipTitle(QStringLiteral("Tern"));
    m_tray->setToolTipSubTitle(QStringLiteral("Link router"));
    m_tray->setIconByName(QStringLiteral("app.tern.Tern"));
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

    connect(m_ruleModel, &RuleModel::rulesChanged, this, [this]() {
        m_config.rules = m_ruleModel->rules();
        persist();
    });
}

QString Controller::appVersion() const
{
    return QStringLiteral(TERN_VERSION_STRING);
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
    return QString::fromUtf8(p.readAllStandardOutput()).trimmed() == QLatin1String("app.tern.Tern.desktop");
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
    if (!rest.isEmpty() && rest.first().contains(QLatin1String("tern"))) {
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

    m_destinationLadder = Tern::destinationLadder(m_click.matchUrl);
    const Target *suggested = d.action == Decision::Action::Launch ? &d.target : nullptr;
    m_destinationIndex = Tern::suggestedLadderIndex(m_click.matchUrl, suggested, m_config.remembered);

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
    hidePicker();
    if (t->id == QLatin1String("action:copy")) {
        copyCurrent();
        return;
    }
    launch(*t, QStringLiteral("picker"));
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
                      {QStringLiteral("default"), QStringLiteral("app.tern.Tern.desktop"), QStringLiteral("x-scheme-handler/http")});
    QProcess::execute(QStringLiteral("xdg-mime"),
                      {QStringLiteral("default"), QStringLiteral("app.tern.Tern.desktop"), QStringLiteral("x-scheme-handler/https")});
    QProcess::execute(QStringLiteral("xdg-settings"),
                      {QStringLiteral("set"), QStringLiteral("default-web-browser"), QStringLiteral("app.tern.Tern.desktop")});
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

void Controller::addCustomTarget(const QString &name, const QString &command)
{
    Target t;
    QString error;
    if (!parseCustomCommand(command, &t, &error)) {
        qWarning() << "Tern: rejected custom handler:" << error;
        return;
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
    launch(d.target, d.reason);
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

void Controller::launch(const Target &target, const QString &reason)
{
    if (target.id.isEmpty()) {
        showPicker();
        return;
    }
    if (!isSafeOpenUrl(m_click.openUrl)) {
        auto *n = new KNotification(QStringLiteral("opened"), KNotification::CloseOnTimeout, this);
        n->setTitle(QStringLiteral("Tern blocked this link"));
        n->setText(QStringLiteral("Only http and https links can be opened."));
        n->setIconName(QStringLiteral("security-high"));
        n->sendEvent();
        return;
    }
    if (!launchTarget(target, m_click.openUrl)) {
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

void Controller::toast(const Target &target, const QString &reason)
{
    auto *n = new KNotification(QStringLiteral("opened"), KNotification::CloseOnTimeout, this);
    n->setTitle(QStringLiteral("Opened in %1").arg(target.displayName()));
    n->setText(m_click.host.isEmpty() ? QStringLiteral("Link opened") : m_click.host);
    n->setIconName(target.icon.isEmpty() ? QStringLiteral("app.tern.Tern") : target.icon);
    Q_UNUSED(reason);
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
            qWarning() << "Tern picker:" << w.toString();
        }
    });
    m_pickerEngine->rootContext()->setContextProperty(QStringLiteral("controller"), this);
    m_pickerEngine->loadFromModule(QStringLiteral("app.tern"), QStringLiteral("Picker"));
    if (m_pickerEngine->rootObjects().isEmpty()) {
        qWarning() << "Tern: picker QML failed to load";
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
            qWarning() << "Tern settings:" << w.toString();
        }
    });
    m_settingsEngine->rootContext()->setContextProperty(QStringLiteral("controller"), this);
    m_settingsEngine->loadFromModule(QStringLiteral("app.tern"), QStringLiteral("Settings"));
    if (m_settingsEngine->rootObjects().isEmpty()) {
        qWarning() << "Tern: settings QML failed to load";
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
            hideHold();
            launch(m_holdTarget, m_holdReason);
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
    hideHold();
    launch(m_holdTarget, m_holdReason);
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
            qWarning() << "Tern hold:" << w.toString();
        }
    });
    m_holdEngine->rootContext()->setContextProperty(QStringLiteral("controller"), this);
    m_holdEngine->loadFromModule(QStringLiteral("app.tern"), QStringLiteral("Hold"));
    if (m_holdEngine->rootObjects().isEmpty()) {
        qWarning() << "Tern: hold QML failed to load";
        return;
    }
    m_holdWindow = qobject_cast<QWindow *>(m_holdEngine->rootObjects().constFirst());
    if (m_holdWindow) {
        configureLayerShell(m_holdWindow, QStringLiteral("tern-hold"));
    }
}

} // namespace Tern
