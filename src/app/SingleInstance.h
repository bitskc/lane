#pragma once

#include <QObject>
#include <QString>

#include <functional>

class QJsonObject;
class QLocalServer;

namespace Lane
{

// Local-socket based single-instance guard, replacing KDBusService's
// uniqueness half (the D-Bus name claim in DBusActivation.h covers the
// rest, i.e. the systemd readiness signal and DBusActivatable handlers).
class SingleInstance : public QObject
{
    Q_OBJECT
public:
    explicit SingleInstance(QObject *parent = nullptr);

    // Tries to become the primary instance. Returns true if this process
    // is primary (it now owns the listening socket; newConnection will
    // invoke `onMessage` for every later invocation's forwarded payload).
    // Returns false after this process has forwarded its own invocation
    // (args/workingDir/activationToken) to the already-running primary;
    // the caller must exit(0) in that case. Exits the process itself with
    // a nonzero status on unrecoverable failure (a stale socket that
    // cannot be cleared, or a fresh listen() that still fails afterward).
    bool claim();

    // Invoked on the primary instance for every payload a later `lane`
    // invocation forwards. Payload shape:
    // {"args": [...], "workingDir": "...", "activationToken": "..."}
    std::function<void(const QJsonObject &)> onMessage;

private:
    void handleNewConnection();

    QLocalServer *m_server = nullptr;
};

} // namespace Lane
