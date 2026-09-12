#pragma once

#include "PickerModel.h"
#include "RuleModel.h"
#include "TargetModel.h"
#include "UpdateChecker.h"
#include "core/types.h"
#include "core/pipeline.h"

#include <QObject>
#include <QPointer>
#include <QQmlApplicationEngine>
#include <QVariantAnimation>
#include <QWindow>

class KStatusNotifierItem;

namespace Lane
{

class Controller : public QObject
{
    Q_OBJECT
    Q_PROPERTY(PickerModel *pickerModel READ pickerModel CONSTANT)
    Q_PROPERTY(TargetModel *targetModel READ targetModel CONSTANT)
    Q_PROPERTY(RuleModel *ruleModel READ ruleModel CONSTANT)
    Q_PROPERTY(QString appVersion READ appVersion CONSTANT)
    Q_PROPERTY(QString currentUrl READ currentUrl NOTIFY currentChanged)
    Q_PROPERTY(QString currentHost READ currentHost NOTIFY currentChanged)
    Q_PROPERTY(QString currentPrettyUrl READ currentPrettyUrl NOTIFY currentChanged)
    Q_PROPERTY(bool currentSecure READ currentSecure NOTIFY currentChanged)
    Q_PROPERTY(bool alwaysForHost READ alwaysForHost WRITE setAlwaysForHost NOTIFY currentChanged)
    Q_PROPERTY(bool isDefaultBrowser READ isDefaultBrowser NOTIFY defaultBrowserChanged)
    Q_PROPERTY(QString pickerPolicy READ pickerPolicy WRITE setPickerPolicy NOTIFY settingsChanged)
    Q_PROPERTY(bool preferPwa READ preferPwa WRITE setPreferPwa NOTIFY settingsChanged)
    Q_PROPERTY(bool toastEnabled READ toastEnabled WRITE setToastEnabled NOTIFY settingsChanged)
    Q_PROPERTY(bool unwrapO365 READ unwrapO365 WRITE setUnwrapO365 NOTIFY settingsChanged)
    Q_PROPERTY(bool unshorten READ unshorten WRITE setUnshorten NOTIFY settingsChanged)
    Q_PROPERTY(bool autostart READ autostart WRITE setAutostartEnabled NOTIFY settingsChanged)
    Q_PROPERTY(bool closeOnFocusLoss READ closeOnFocusLoss WRITE setCloseOnFocusLoss NOTIFY settingsChanged)
    Q_PROPERTY(bool showUrl READ showUrl WRITE setShowUrl NOTIFY settingsChanged)
    Q_PROPERTY(QString defaultTargetId READ defaultTargetId WRITE setDefaultTargetId NOTIFY settingsChanged)
    Q_PROPERTY(int targetCount READ targetCount NOTIFY settingsChanged)
    Q_PROPERTY(QStringList targetIds READ targetIds NOTIFY settingsChanged)
    Q_PROPERTY(QStringList targetNames READ targetNames NOTIFY settingsChanged)
    Q_PROPERTY(QStringList rememberedHosts READ rememberedHosts NOTIFY settingsChanged)
    Q_PROPERTY(bool holdAutoOpen READ holdAutoOpen WRITE setHoldAutoOpen NOTIFY settingsChanged)
    Q_PROPERTY(int holdMs READ holdMs WRITE setHoldMs NOTIFY settingsChanged)
    Q_PROPERTY(qreal holdProgress READ holdProgress NOTIFY holdProgressChanged)
    Q_PROPERTY(QString holdTargetName READ holdTargetName NOTIFY holdChanged)
    Q_PROPERTY(QString holdDestinationKey READ holdDestinationKey NOTIFY holdChanged)
    Q_PROPERTY(QStringList destinationLadder READ destinationLadder NOTIFY currentChanged)
    Q_PROPERTY(int destinationIndex READ destinationIndex WRITE setDestinationIndex NOTIFY currentChanged)
    Q_PROPERTY(QString currentDestinationKey READ currentDestinationKey NOTIFY currentChanged)
    Q_PROPERTY(QString projectUrl READ projectUrl CONSTANT)
    Q_PROPERTY(QString updateCheckState READ updateCheckState NOTIFY updateStateChanged)
    Q_PROPERTY(QString updateLatestVersion READ updateLatestVersion NOTIFY updateStateChanged)
    Q_PROPERTY(QString updateReleaseUrl READ updateReleaseUrl NOTIFY updateStateChanged)
    Q_PROPERTY(QString updateErrorMessage READ updateErrorMessage NOTIFY updateStateChanged)
    Q_PROPERTY(QString updateLastChecked READ updateLastChecked NOTIFY updateStateChanged)
public:
    explicit Controller(QObject *parent = nullptr);

    PickerModel *pickerModel() const { return m_pickerModel; }
    TargetModel *targetModel() const { return m_targetModel; }
    RuleModel *ruleModel() const { return m_ruleModel; }
    QString appVersion() const;
    QString projectUrl() const;
    QString updateCheckState() const;
    QString updateLatestVersion() const { return m_updateChecker->latestVersion(); }
    QString updateReleaseUrl() const { return m_updateChecker->releaseUrl(); }
    QString updateErrorMessage() const { return m_updateChecker->errorMessage(); }
    QString updateLastChecked() const;

    QString currentUrl() const { return m_click.openUrl; }
    QString currentHost() const { return m_click.host; }
    QString currentPrettyUrl() const;
    bool currentSecure() const;
    bool alwaysForHost() const { return m_alwaysForHost; }
    void setAlwaysForHost(bool on);

    bool isDefaultBrowser() const;
    QString pickerPolicy() const;
    void setPickerPolicy(const QString &p);
    bool preferPwa() const { return m_config.preferPwa; }
    void setPreferPwa(bool on);
    bool toastEnabled() const { return m_config.toast; }
    void setToastEnabled(bool on);
    bool unwrapO365() const { return m_config.unwrapO365; }
    void setUnwrapO365(bool on);
    bool unshorten() const { return m_config.unshorten; }
    void setUnshorten(bool on);
    bool autostart() const { return m_config.autostart; }
    void setAutostartEnabled(bool on);
    bool closeOnFocusLoss() const { return m_config.closeOnFocusLoss; }
    void setCloseOnFocusLoss(bool on);
    QStringList rememberedHosts() const;

    bool holdAutoOpen() const { return m_config.holdAutoOpen; }
    void setHoldAutoOpen(bool on);
    int holdMs() const { return m_config.holdMs; }
    void setHoldMs(int ms);
    qreal holdProgress() const { return m_holdProgress; }
    QString holdTargetName() const { return m_holdTargetName; }
    QString holdDestinationKey() const { return m_holdDestinationKey; }
    QStringList destinationLadder() const { return m_destinationLadder; }
    int destinationIndex() const { return m_destinationIndex; }
    void setDestinationIndex(int idx);
    QString currentDestinationKey() const;
    bool showUrl() const { return m_config.showUrl; }
    void setShowUrl(bool on);
    QString defaultTargetId() const { return m_config.defaultTargetId; }
    void setDefaultTargetId(const QString &id);
    int targetCount() const { return m_targets.size(); }
    QStringList targetIds() const;
    QStringList targetNames() const;
    Q_INVOKABLE void confirmHold();
    Q_INVOKABLE void cancelHold();

    Q_INVOKABLE void handleArgs(const QStringList &args);
    Q_INVOKABLE void openUrl(const QString &url, bool forcePicker = false);
    Q_INVOKABLE void pick(int row);
    Q_INVOKABLE void pickId(const QString &id);
    Q_INVOKABLE void cancelPicker();
    Q_INVOKABLE void copyCurrent();
    Q_INVOKABLE void openSettings();
    Q_INVOKABLE void rediscover();
    Q_INVOKABLE void makeDefaultBrowser();
    Q_INVOKABLE void hideTarget(const QString &id, bool hidden);
    Q_INVOKABLE void save();
    Q_INVOKABLE QString displayNameFor(const QString &id) const;
    Q_INVOKABLE QString rememberedTarget(const QString &host) const;
    Q_INVOKABLE void forgetHost(const QString &host);
    Q_INVOKABLE QString addCustomTarget(const QString &name, const QString &command);
    Q_INVOKABLE bool targetExists(const QString &id) const;
    Q_INVOKABLE QStringList danglingRememberedHosts() const;
    Q_INVOKABLE void clearDeadRemembered();
    Q_INVOKABLE void removeCustomTarget(const QString &id);
    Q_INVOKABLE void renameTarget(const QString &id, const QString &name);
    Q_INVOKABLE void moveTarget(const QString &id, int newIndexInKind);
    Q_INVOKABLE void checkForUpdates();
    Q_INVOKABLE void openExternalUrl(const QString &url);
Q_SIGNALS:
    void currentChanged();
    void settingsChanged();
    void defaultBrowserChanged();
    void pickerVisibleChanged(bool visible);
    void holdProgressChanged();
    void holdChanged();
    void updateStateChanged();
private:
    void reload();
    void persist();
    void applyDecision(const Decision &d);
    void showPicker();
    void hidePicker();
    void launch(const Target &target, const QString &reason, const QString &activationToken = QString());
    // Requests a fresh xdg-activation token from `window` (its most recent
    // input event authorizes the token) and launches once the compositor
    // answers, or immediately once a short deadline passes with no answer.
    // `window` is null for a launch with no Lane-owned surface involved
    // (nothing was ever shown), in which case whatever inbound activation
    // token this click arrived with (see openUrl()) is used instead.
    void requestActivationAndLaunch(const Target &target, const QString &reason, QWindow *window);
    void toast(const Target &target, const QString &reason);
    void notifyBlocked();
    void ensurePickerEngine();
    void ensureSettingsEngine();
    void ensureHoldEngine();
    void configureLayerShell(QWindow *window, const QString &scope = QStringLiteral("lane-picker"));
    bool shouldHold(const QString &reason) const;
    void startHold(const Target &target, const QString &reason, const QString &memoryKey);
    void hideHold();
    UnshortenFn unshortenFn() const;

    Config m_config;
    QString m_configPath;
    QList<Target> m_targets;
    Click m_click;
    bool m_alwaysForHost = false;
    // Whatever inbound XDG_ACTIVATION_TOKEN this click's openUrl() call
    // carried (from KDBusService relaying a caller's token, or inherited at
    // process start), consumed at most once per click. Used only for a
    // direct silent launch; the picker and hold paths request their own
    // fresh token instead (see requestActivationAndLaunch()).
    QString m_pendingActivationToken;

    PickerModel *m_pickerModel = nullptr;
    TargetModel *m_targetModel = nullptr;
    RuleModel *m_ruleModel = nullptr;
    UpdateChecker *m_updateChecker = nullptr;

    QQmlApplicationEngine *m_pickerEngine = nullptr;
    QQmlApplicationEngine *m_settingsEngine = nullptr;
    QQmlApplicationEngine *m_holdEngine = nullptr;
    QPointer<QWindow> m_pickerWindow;
    QPointer<QWindow> m_settingsWindow;
    QPointer<QWindow> m_holdWindow;
    KStatusNotifierItem *m_tray = nullptr;

    QVariantAnimation *m_holdAnimation = nullptr;
    Target m_holdTarget;
    QString m_holdReason;
    QString m_holdMemoryKey;
    QString m_holdTargetName;
    QString m_holdDestinationKey;
    qreal m_holdProgress = 0;
    QStringList m_destinationLadder;
    int m_destinationIndex = 0;
    // Re-entrancy guard for openUrl: unshortenSync() runs a nested
    // QEventLoop::exec() during which a second D-Bus openRequested can
    // re-enter openUrl and overwrite m_click mid-pipeline.  Pending
    // calls are queued and drained one at a time after applyDecision.
    struct PendingUrl {
        QString url;
        bool forcePicker = false;
        QString activationToken;
    };
    QList<PendingUrl> m_pendingUrls;
    bool m_inOpenUrl = false;
};

} // namespace Lane
