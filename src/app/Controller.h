#pragma once

#include "ClickGate.h"
#include "PickerModel.h"
#include "RuleModel.h"
#include "TargetModel.h"
#include "UpdateChecker.h"
#include "DefaultBrowserWatcher.h"
#include "core/types.h"
#include "platform/platform.h"
#include "core/pipeline.h"

#include <QHash>
#include <QElapsedTimer>
#include <QObject>
#include <QPointer>
#include <QQmlApplicationEngine>
#include <QVariant>
#include <QVariantAnimation>
#include <QWindow>
#include <QTimer>

class QSystemTrayIcon;

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
    Q_PROPERTY(bool quitShortcutEnabled READ quitShortcutEnabled CONSTANT)
    Q_PROPERTY(QString currentUrl READ currentUrl NOTIFY currentChanged)
    Q_PROPERTY(QString currentHost READ currentHost NOTIFY currentChanged)
    Q_PROPERTY(QString currentPrettyUrl READ currentPrettyUrl NOTIFY currentChanged)
    Q_PROPERTY(bool currentSecure READ currentSecure NOTIFY currentChanged)
    Q_PROPERTY(bool alwaysForHost READ alwaysForHost WRITE setAlwaysForHost NOTIFY currentChanged)
    Q_PROPERTY(bool isDefaultBrowser READ isDefaultBrowser NOTIFY defaultBrowserChanged)
    // Name of the app currently holding http/https (e.g. "OpenIn",
    // "Safari"); empty when the OS cannot report one.
    Q_PROPERTY(QString defaultHandlerName READ defaultHandlerName NOTIFY defaultBrowserChanged)
    // One of registered, partial, other-copy, declined, unknown: the
    // state machine behind the Overview button and takeover alert.
    Q_PROPERTY(QString defaultHandlerStatus READ defaultHandlerStatus NOTIFY defaultBrowserChanged)
    // Live read of the OS reduce-motion setting; gates the Hold ring and
    // dim-fade animations. Not NOTIFY: QML reads it at each show.
    Q_PROPERTY(bool reduceMotion READ reduceMotion NOTIFY settingsChanged)
    Q_PROPERTY(QString pickerPolicy READ pickerPolicy WRITE setPickerPolicy NOTIFY settingsChanged)
    Q_PROPERTY(bool preferPwa READ preferPwa WRITE setPreferPwa NOTIFY settingsChanged)
    Q_PROPERTY(bool toastEnabled READ toastEnabled WRITE setToastEnabled NOTIFY settingsChanged)
    Q_PROPERTY(bool unwrapO365 READ unwrapO365 WRITE setUnwrapO365 NOTIFY settingsChanged)
    Q_PROPERTY(bool unshorten READ unshorten WRITE setUnshorten NOTIFY settingsChanged)
    Q_PROPERTY(bool stripTrackingParams READ stripTrackingParams WRITE setStripTrackingParams NOTIFY settingsChanged)
    Q_PROPERTY(bool watchdogEnabled READ watchdogEnabled WRITE setWatchdogEnabled NOTIFY settingsChanged)
    Q_PROPERTY(bool autostart READ autostart WRITE setAutostartEnabled NOTIFY settingsChanged)
    // macOS notification denial, read live; drives the Overview banner.
    Q_PROPERTY(bool notificationsDenied READ notificationsDenied NOTIFY settingsChanged)
    // Human-readable autostart blocker (requires-approval, unsigned
    // build) for the Settings toggle description; empty when fine.
    Q_PROPERTY(QString autostartMessage READ autostartMessage NOTIFY settingsChanged)
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
    ~Controller() override;

    PickerModel *pickerModel() const { return m_pickerModel; }
    TargetModel *targetModel() const { return m_targetModel; }
    RuleModel *ruleModel() const { return m_ruleModel; }
    QString appVersion() const;
    // StandardKey.Quit shortcuts exist for the macOS status-item quit
    // path (spec: accessory app must offer a discoverable quit). On
    // Linux the daemon owns routing and shortcut-quit is off-spec.
    bool quitShortcutEnabled() const {
#ifdef Q_OS_MACOS
        return true;
#else
        return false;
#endif
    }
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
    QString defaultHandlerName() const { return m_defaultHandlerName; }
    QString defaultHandlerStatus() const { return m_defaultHandlerStatus; }
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
    bool reduceMotion() const { return platform()->prefersReducedMotion(); }
    bool notificationsDenied() const { return platform()->notificationsDenied(); }
    QString autostartMessage() const;
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
    // Per-Activity fallback (config.activityDefaults): empty when the
    // Activity has no fallback set, in which case defaultTargetId applies.
    // Plain Q_INVOKABLE, not a Q_PROPERTY: keyed by activityId, same shape
    // as rememberedTarget()/forgetHost() below.
    Q_INVOKABLE QString activityDefaultTarget(const QString &activityId) const;
    Q_INVOKABLE void setActivityDefaultTarget(const QString &activityId, const QString &targetId);
    int targetCount() const { return m_targets.size(); }
    QStringList targetIds() const;
    QStringList targetNames() const;
    Q_INVOKABLE void confirmHold();
    Q_INVOKABLE void cancelHold();

    Q_INVOKABLE void handleArgs(const QStringList &args);
    Q_INVOKABLE void openUrl(const QString &url, bool forcePicker = false);
    // Reads the system clipboard (trimmed) and routes it when it parses
    // as an http(s) URL; otherwise does nothing (tray notice on Linux,
    // log line on macOS). Wired to --clipboard, the tray menu entry, and
    // the Linux desktop Action.
    Q_INVOKABLE void openClipboardUrl(bool forcePicker = false);
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
    Q_INVOKABLE void emailCurrent();
    Q_INVOKABLE void openSettings();
    Q_INVOKABLE void rediscover();
    Q_INVOKABLE void makeDefaultBrowser();

    // Cancels a pending pick or hold, or quits the app when nothing is
    // pending (decision D2; reachable from the status item and ⌘Q).
    Q_INVOKABLE void requestQuit();
    // Opens Settings once on a first run: immediately when there is no
    // link, or after the first pick/launch when a link started Lane.
    // No-op after the first call.
    Q_INVOKABLE void maybeFirstRunSettings();
    // Test seam, not a QML API: the flag is otherwise only observable by
    // whether Settings happens to open.
    bool firstRunPending() const { return m_firstRun; }
    QWindow *settingsWindow() const { return m_settingsWindow; }
    QWindow *pickerWindow() const { return m_pickerWindow; }
    Q_INVOKABLE void hideTarget(const QString &id, bool hidden);
    Q_INVOKABLE QString displayNameFor(const QString &id) const;
    Q_INVOKABLE QString rememberedTarget(const QString &host) const;
    Q_INVOKABLE void forgetHost(const QString &host);
    Q_INVOKABLE QString addCustomTarget(const QString &name, const QString &command);
    Q_INVOKABLE bool targetExists(const QString &id) const;
    Q_INVOKABLE QStringList danglingRememberedHosts() const;
    Q_INVOKABLE void clearDeadRemembered();
    Q_INVOKABLE void removeCustomTarget(const QString &id);
    Q_INVOKABLE void renameTarget(const QString &id, const QString &name);
    Q_INVOKABLE void moveTarget(const QString &id, int newIndex);
    Q_INVOKABLE void checkForUpdates();
    Q_INVOKABLE void openExternalUrl(const QString &url);
Q_SIGNALS:
    void currentChanged();
    void settingsChanged();
    void defaultBrowserChanged();
    void holdProgressChanged();
    void holdChanged();
    void updateStateChanged();
    void pickerNoticeChanged();
    void activitiesChanged();
private:
    void reload();
    void persist();
    void applyDecision(const Decision &d);
    // Returns the trimmed clipboard text when it parses as an http(s)
    // URL, empty otherwise. Drives the tray action's enabled state and
    // openClipboardUrl().
    QString clipboardUrlCandidate() const;
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
    QWindow *ensureEngine(QQmlApplicationEngine *&engine, const QString &module, const QString &name, const QString &warnPrefix);
    void ensurePickerEngine();
    void ensureSettingsEngine();
    void ensureHoldEngine();
    bool shouldHold(const QString &reason) const;
    void startHold(const Target &target, const QString &reason, const QString &memoryKey);
    void hideHold();
    UnshortenFn unshortenFn() const;
    void refreshDefaultBrowserState();
    // Runs after the debounced watcher fires: compares the last-known
    // default state against a fresh check and raises the takeover alert
    // only on a real loss.
    void onMimeappsChanged();
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
    // Cached result of the last HTTP/HTTPS handler check (see
    // refreshDefaultBrowserState()); isDefaultBrowser() only reads this,
    // it never spawns xdg-mime itself.
    bool m_isDefaultBrowser = false;
    // Watches mimeapps.list for a browser update or app that grabs the
    // http/https handler; owned by the takeover alert it can raise.
    DefaultBrowserWatcher *m_defaultWatcher = nullptr;
    // macOS has no mimeapps.list to watch; this timer polls
    // defaultHandlerState() every 60 s while watchdogEnabled so a
    // browser update that takes over http/https still raises the alert.
    QTimer *m_defaultPoll = nullptr;
    // Cached halves of the last defaultHandlerState() call, kept next to
    // m_isDefaultBrowser for the Overview and takeover alert.
    QString m_defaultHandlerName;
    QString m_defaultHandlerStatus = QStringLiteral("unknown");
    // Whatever inbound XDG_ACTIVATION_TOKEN this click's openUrl() call
    // carried (from KDBusService relaying a caller's token, or inherited at
    // process start), consumed at most once per click. Used only for a
    // direct silent launch; the picker and hold paths request their own
    // fresh token instead (see requestActivationAndLaunch()).
    QString m_pendingActivationToken;
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
    QSystemTrayIcon *m_tray = nullptr;

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
    // Ordering rules for one in-flight click (queue, deferred commands,
    // abandon at quit). unshortenSync() runs a nested event loop during
    // which another link, --rediscover, --settings or a quit can arrive;
    // ClickGate decides what each does and Controller performs it.
    ClickGate m_gate;

    // Rate-limit for rediscover(): every caller (tray, --rediscover argv,
    // D-Bus Rescan action, second-instance CommandLine) funnels here, and
    // any session-bus client can spam it; collapse bursts to one rescan
    // per kRediscoverMinInterval.
    QElapsedTimer m_rediscoverTimer;

    // True until the first interaction completes (pick, launch, or a
    // first launch with no link): opens Settings once, per decision D2's
    // onboarding rule. Set in the ctor from "no config file on disk".
    bool m_firstRun = false;
};

} // namespace Lane
