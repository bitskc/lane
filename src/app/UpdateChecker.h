#pragma once

#include <QDateTime>
#include <QObject>
#include <QPointer>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;
class QUrl;

namespace Lane
{

// Manual, asynchronous "check for updates" against the GitHub releases API.
// Never runs on a timer and never runs on startup; check() only ever fires
// because the user pressed a button. Uses QNetworkAccessManager signals
// throughout, never a nested QEventLoop.
class UpdateChecker : public QObject
{
    Q_OBJECT
public:
    enum class Status {
        Idle,
        Checking,
        UpToDate,
        UpdateAvailable,
        Failed,
    };

    explicit UpdateChecker(QString currentVersion, QObject *parent = nullptr);

    Status status() const { return m_status; }
    QString latestVersion() const { return m_latestVersion; }
    QString releaseUrl() const { return m_releaseUrl; }
    QString errorMessage() const { return m_errorMessage; }
    QDateTime lastCheckedAt() const { return m_lastCheckedAt; }

public Q_SLOTS:
    // A check already in flight is left alone; a second click never starts
    // a second request.
    void check();

Q_SIGNALS:
    void statusChanged();

private:
    void startRequest(const QUrl &url, int redirectsLeft);
    void handleReply(QNetworkReply *reply, int redirectsLeft);
    void setStatus(Status status);
    void fail(const QString &message);

    QNetworkAccessManager *m_nam;
    QPointer<QNetworkReply> m_reply;
    QString m_currentVersion;
    Status m_status = Status::Idle;
    QString m_latestVersion;
    QString m_releaseUrl;
    QString m_errorMessage;
    QDateTime m_lastCheckedAt;
};

} // namespace Lane
