#include "CrashLog.h"

#include <QByteArray>
#include <QDir>
#include <QMutex>
#include <QMutexLocker>
#include <QStandardPaths>
#include <QString>
#include <QtGlobal>

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <csignal>
#include <ctime>

#include <fcntl.h>
#include <unistd.h>

namespace Lane
{
namespace CrashLog
{
namespace
{

constexpr int kRingSize = 200;

QMutex &ringMutex()
{
    static QMutex mutex;
    return mutex;
}

QByteArray *ringBuffer()
{
    static QByteArray lines[kRingSize];
    return lines;
}

std::atomic<quint64> &ringCounter()
{
    static std::atomic<quint64> counter{0};
    return counter;
}

// Directory for crash logs, resolved and created once at install() time
// (QStandardPaths/QDir are not safe to call from a signal handler); a
// fixed-size byte buffer so the crash path can be built with snprintf
// alone, with no heap allocation, when a signal actually fires.
char g_logDir[1024] = {0};

void ensureLogDir()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    const QByteArray utf8 = dir.toUtf8();
    const qsizetype n = qMin<qsizetype>(utf8.size(), qsizetype(sizeof(g_logDir) - 1));
    std::memcpy(g_logDir, utf8.constData(), size_t(n));
    g_logDir[n] = '\0';
}

// Best-effort: writes `reason` followed by the ring buffer's contents to
// a fresh crash-<epoch>.log file. Called from std::terminate and from
// fatal-signal handlers, so it sticks to plain POSIX I/O rather than Qt
// file APIs.
void writeCrashLog(const char *reason)
{
    if (g_logDir[0] == '\0') {
        return;
    }
    char path[1200];
    std::snprintf(path, sizeof(path), "%s/crash-%lld.log", g_logDir, static_cast<long long>(::time(nullptr)));

    const int fd = ::open(path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd < 0) {
        return;
    }

    if (reason) {
        ::write(fd, reason, std::strlen(reason));
        ::write(fd, "\n", 1);
    }

    {
        QMutexLocker locker(&ringMutex());
        const quint64 total = ringCounter().load();
        const quint64 count = qMin<quint64>(total, quint64(kRingSize));
        const quint64 start = total > quint64(kRingSize) ? total - quint64(kRingSize) : 0;
        QByteArray *lines = ringBuffer();
        for (quint64 i = 0; i < count; ++i) {
            const QByteArray &line = lines[(start + i) % quint64(kRingSize)];
            if (!line.isEmpty()) {
                ::write(fd, line.constData(), size_t(line.size()));
                ::write(fd, "\n", 1);
            }
        }
    }

    ::close(fd);
}

void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    const QString formatted = qFormatLogMessage(type, context, msg);
    const quint64 idx = ringCounter().fetch_add(1);
    {
        QMutexLocker locker(&ringMutex());
        ringBuffer()[idx % quint64(kRingSize)] = formatted.toUtf8();
    }
    std::fprintf(stderr, "%s\n", qPrintable(formatted));

    // Matches Qt's default handler: a fatal message aborts the process.
    if (type == QtFatalMsg) {
        std::abort();
    }
}

[[noreturn]] void terminateHandler()
{
    writeCrashLog("std::terminate() called (unhandled exception)");
    std::abort();
}

void fatalSignalHandler(int sig)
{
    const char *name = "unknown fatal signal";
    switch (sig) {
    case SIGSEGV: name = "SIGSEGV"; break;
    case SIGABRT: name = "SIGABRT"; break;
    case SIGBUS: name = "SIGBUS"; break;
    case SIGILL: name = "SIGILL"; break;
    case SIGFPE: name = "SIGFPE"; break;
    default: break;
    }
    writeCrashLog(name);

    // Restore the default disposition and re-raise: never suppress a
    // core dump, and never return into whatever corrupted the process.
    std::signal(sig, SIG_DFL);
    std::raise(sig);
}

} // namespace

void install()
{
    ensureLogDir();
    qInstallMessageHandler(messageHandler);
    std::set_terminate(terminateHandler);
    std::signal(SIGSEGV, fatalSignalHandler);
    std::signal(SIGABRT, fatalSignalHandler);
    std::signal(SIGBUS, fatalSignalHandler);
    std::signal(SIGILL, fatalSignalHandler);
    std::signal(SIGFPE, fatalSignalHandler);
}

} // namespace CrashLog
} // namespace Lane
