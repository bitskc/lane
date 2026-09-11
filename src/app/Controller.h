#pragma once

#include "PickerModel.h"
#include "RuleModel.h"
#include "TargetModel.h"
#include "core/types.h"
#include "core/pipeline.h"

#include <QObject>
#include <QPointer>
#include <QQmlApplicationEngine>
#include <QWindow>

class KStatusNotifierItem;

namespace Tern
{

class Controller : public QObject
{
    Q_OBJECT
    Q_PROPERTY(PickerModel *pickerModel READ pickerModel CONSTANT)
    Q_PROPERTY(TargetModel *targetModel READ targetModel CONSTANT)
    Q_PROPERTY(RuleModel *ruleModel READ ruleModel CONSTANT)
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
public:
    explicit Controller(QObject *parent = nullptr);

    PickerModel *pickerModel() const { return m_pickerModel; }
    TargetModel *targetModel() const { return m_targetModel; }
    RuleModel *ruleModel() const { return m_ruleModel; }

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
    bool showUrl() const { return m_config.showUrl; }
    void setShowUrl(bool on);
    QString defaultTargetId() const { return m_config.defaultTargetId; }
    void setDefaultTargetId(const QString &id);
    int targetCount() const { return m_targets.size(); }
    QStringList targetIds() const;
    QStringList targetNames() const;
    QStringList rememberedHosts() const;

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
    Q_INVOKABLE void addCustomTarget(const QString &name, const QString &command);
    Q_INVOKABLE void removeCustomTarget(const QString &id);
Q_SIGNALS:
    void currentChanged();
    void settingsChanged();
    void defaultBrowserChanged();
    void pickerVisibleChanged(bool visible);

private:
    void reload();
    void persist();
    void applyDecision(const Decision &d);
    void showPicker();
    void hidePicker();
    void launch(const Target &target, const QString &reason);
    void toast(const Target &target, const QString &reason);
    void ensurePickerEngine();
    void ensureSettingsEngine();
    void configureLayerShell(QWindow *window);
    UnshortenFn unshortenFn() const;

    Config m_config;
    QString m_configPath;
    QList<Target> m_targets;
    Click m_click;
    bool m_alwaysForHost = false;

    PickerModel *m_pickerModel = nullptr;
    TargetModel *m_targetModel = nullptr;
    RuleModel *m_ruleModel = nullptr;

    QQmlApplicationEngine *m_pickerEngine = nullptr;
    QQmlApplicationEngine *m_settingsEngine = nullptr;
    QPointer<QWindow> m_pickerWindow;
    QPointer<QWindow> m_settingsWindow;
    KStatusNotifierItem *m_tray = nullptr;
};

} // namespace Tern
