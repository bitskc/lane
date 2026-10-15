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
//
// Wire protocol, both halves implemented here: a forwarding invocation
// writes one compact JSON object terminated by '\n' and waits for the
// primary's one-line ack ("ok" = accepted, anything else = rejected).
// The terminator makes a payload that arrives in several reads
// recoverable, and the ack is what lets a forwarding process tell
// "delivered" from "the primary died mid-handshake" instead of
// exiting 0 on a message nobody ever read.

// The runtime socket path claim() binds and forwarding invocations
// connect to. Exported for tests. On macOS the path is
// <confstr(_CS_DARWIN_USER_TEMP_DIR)>lane-<uid>.sock and an empty return
// means it exceeded 103 bytes (callers must fail, not pick a fallback);
// XDG_RUNTIME_DIR overrides the base directory so tests stay sandboxed.
QString singleInstanceSocketPath();


class SingleInstance : public QObject
{
    Q_OBJECT
public:
    explicit SingleInstance(QObject *parent = nullptr);

    // Tries to become the primary instance. Returns true if this process
    // is primary (it now owns the listening socket; newConnection will
    // invoke `onMessage` for every later invocation's forwarded payload).
    // Returns false after this process has forwarded its own invocation
    // (args/workingDir/activationToken) to the already-running primary
    // *and that primary acknowledged it*; the caller must exit(0) in
    // that case. Blocks for up to a few seconds while forwarding, since
    // the primary acks from its event loop and may be busy with its own
    // startup scan.
    //
    // Exits the process itself with a nonzero status on unrecoverable
    // failure: a live primary that accepts the connection but never
    // acknowledges the payload (rebinding over it would orphan it), or a
    // bind that fails with nobody else listening.
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
