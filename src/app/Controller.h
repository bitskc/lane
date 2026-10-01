#pragma once

#include "PickerModel.h"
#include "RuleModel.h"
#include "TargetModel.h"
#include "UpdateChecker.h"
#include "DefaultBrowserWatcher.h"
#include "core/types.h"
#include "core/pipeline.h"

#include <QHash>
#include <QObject>
#include <QPointer>
#include <QQmlApplicationEngine>
#include <QVariant>
#include <QVariantAnimation>
#include <QWindow>

class KStatusNotifierItem;

#ifdef HAVE_PLASMA_ACTIVITIES
namespace KActivities
{
class Consumer;
}
#endif
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
    Q_PROPERTY(bool stripTrackingParams READ stripTrackingParams WRITE setStripTrackingParams NOTIFY settingsChanged)
    Q_PROPERTY(bool watchdogEnabled READ watchdogEnabled WRITE setWatchdogEnabled NOTIFY settingsChanged)
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
    Q_PROPERTY(QString pickerNotice READ pickerNotice NOTIFY pickerNoticeChanged)
    Q_PROPERTY(bool activityRoutingEnabled READ activityRoutingEnabled WRITE setActivityRoutingEnabled NOTIFY settingsChanged)
    Q_PROPERTY(QString currentActivityId READ currentActivityId NOTIFY activitiesChanged)
    Q_PROPERTY(QString currentActivityName READ currentActivityName NOTIFY activitiesChanged)
    Q_PROPERTY(QVariantList availableActivities READ availableActivities NOTIFY activitiesChanged)
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
    QString pickerNotice() const { return m_pickerNotice; }
    bool activityRoutingEnabled() const { return m_config.activityRoutingEnabled; }
    void setActivityRoutingEnabled(bool on);
    // The Activity the next clicked link will be attributed to. Empty on
    // non-Plasma desktops and before the activities service answers; the
    // picker and rule engine treat empty as "no Activity in effect".
    QString currentActivityId() const { return m_currentActivityId; }
    QString currentActivityName() const;
    // [{id, name}] of every known Plasma Activity, for the per-rule
    // Activity picker in Settings. Empty when the activities service is
    // absent or still synchronizing.
    QVariantList availableActivities() const;

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
    bool stripTrackingParams() const { return m_config.stripTrackingParams; }
    void setStripTrackingParams(bool on);
    bool watchdogEnabled() const { return m_config.watchdogEnabled; }
    void setWatchdogEnabled(bool on);
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
    // Alt+P from the picker: launches the highlighted target's private
    // counterpart (privateCounterpart() in router.cpp) instead of the
    // target itself. Bypasses pickId() entirely so it never touches
    // alwaysForHost/remembered persistence. Fails closed with
    // pickerNotice set (and the picker left open) when the target has no
    // private counterpart.
    Q_INVOKABLE void pickPrivate(const QString &targetId);
    Q_INVOKABLE void clearPickerNotice();
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
    void pickerNoticeChanged();
    void activitiesChanged();
private:
    void reload();
    void persist();
    void applyDecision(const Decision &d);
    void showPicker();
    void hidePicker();
    void launch(const Target &target, const QString &reason, const QString &activationToken, const Click &click);
    // Requests a fresh xdg-activation token from `window` (its most recent
    // input event authorizes the token) and launches once the compositor
    // answers, or immediately once a short deadline passes with no answer.
    // `window` is null for a launch with no Lane-owned surface involved
    // (nothing was ever shown), in which case whatever inbound activation
    // token this click arrived with (see openUrl()) is used instead.
    // `click` is a snapshot taken at decision time: the token request is
    // async and m_click may already belong to a newer click by the time
    // the compositor answers, so the URL to open must never be re-read
    // from m_click at fire time.
    void requestActivationAndLaunch(const Target &target, const QString &reason, QWindow *window, const Click &click);
    void toast(const Target &target, const QString &reason, const QString &host);
    void notifyBlocked();
    void ensurePickerEngine();
    void ensureSettingsEngine();
    void ensureHoldEngine();
    void configureLayerShell(QWindow *window, const QString &scope = QStringLiteral("lane-picker"));
    bool shouldHold(const QString &reason) const;
    void startHold(const Target &target, const QString &reason, const QString &memoryKey);
    void hideHold();
    UnshortenFn unshortenFn() const;
    void refreshDefaultBrowserState();
    // Runs after the debounced watcher fires: compares the last-known
    // default state against a fresh check and raises the takeover alert
    // only on a real loss.
    void onMimeappsChanged();
    // Automatic counterpart to the manual clearDeadRemembered() above:
    // called from reload() on every daemon start/Settings open/Rediscover,
    // so it must not delete a remembered host the first time discovery
    // fails to see its target (see m_rememberedMissCounts below). Only
    // clearDeadRemembered() itself (the Settings "Clear dead" button)
    // prunes immediately, because that is an explicit, user-confirmed
    // action rather than an automatic side effect of reloading.
    void pruneStaleRemembered();
    // Looks up the display name for an Activity ID. Returns an empty
    // string when there is no Plasma activities support or the service
    // has not described the Activity yet.
    QString activityNameFor(const QString &id) const;

    Config m_config;
    QString m_configPath;
    QList<Target> m_targets;
    Click m_click;
    bool m_alwaysForHost = false;
    // Set by pickPrivate() when the highlighted target has no private
    // counterpart (fail closed); read by Picker.qml's inline notice.
    // Cleared on every showPicker() and whenever the QML picker list
    // selection moves to a different target.
    QString m_pickerNotice;
    // Cached result of the last xdg-settings default-browser check (see
    // refreshDefaultBrowserState()); isDefaultBrowser() only reads this,
    // it never spawns xdg-settings itself.
    bool m_isDefaultBrowser = false;
    // Watches mimeapps.list for a browser update or app that grabs the
    // http/https handler; owned by the takeover alert it can raise.
    DefaultBrowserWatcher *m_defaultWatcher = nullptr;
    // Whatever inbound XDG_ACTIVATION_TOKEN this click's openUrl() call
    // carried (from KDBusService relaying a caller's token, or inherited at
    // process start), consumed at most once per click. Used only for a
    // direct silent launch; the picker and hold paths request their own
    // fresh token instead (see requestActivationAndLaunch()).
    QString m_pendingActivationToken;
    // Consecutive reload() passes each currently-dangling remembered-host
    // key (see danglingRememberedKeys()) has stayed dangling. A discovery
    // false negative (Flatpak export dir mid-update, a bare systemd unit
    // with no XDG_DATA_DIRS, etc.) looks identical to the target actually
    // being uninstalled on any single pass, so pruneStaleRemembered() only
    // deletes a remembered host once its miss count reaches the grace
    // period (see shouldPruneRemembered() in router.h); a key that
    // reappears as installed is dropped from this map again. Not part of
    // Controller's public/QML-facing API: entries come and go purely as a
    // side effect of reload().
    QHash<QString, int> m_rememberedMissCounts;
    // Current Plasma Activity snapshot, fed by KActivities::Consumer when
    // Lane is built against PlasmaActivities. Left empty forever
    // otherwise, which keeps every Activity-aware code path inert.
    QString m_currentActivityId;

#ifdef HAVE_PLASMA_ACTIVITIES
    KActivities::Consumer *m_activities = nullptr;
#endif

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
    // Snapshot of the click the running hold belongs to. The hold's
    // finished/confirm paths launch asynchronously, and m_click may have
    // moved on to a newer click by then.
    Click m_holdClick;
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
