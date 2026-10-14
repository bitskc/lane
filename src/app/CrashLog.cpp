#include "CrashLog.h"

#include <QDir>
#include <QMutex>
#include <QMutexLocker>
#include <QStandardPaths>
#include <QString>
#include <QtGlobal>

#include <atomic>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <csignal>
#include <exception>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>

namespace Lane::CrashLog
{
namespace
{
constexpr unsigned kRingSize = 200;
constexpr unsigned kLineBytes = 2048;
static_assert(std::atomic<char>::is_always_lock_free);
static_assert(std::atomic<unsigned>::is_always_lock_free);

// Normal logging is serialized. The signal handler never takes this
// lock or touches a Qt object; it only reads published lock-free bytes.
QMutex g_ringMutex;
struct Line {
    std::atomic<unsigned> sequence{0};
    std::atomic<unsigned> size{0};
    std::atomic<char> bytes[kLineBytes]{};
};
Line g_lines[kRingSize];
std::atomic<unsigned> g_count{0};
char g_prefix[1024]{};
volatile sig_atomic_t g_writing = 0;
alignas(16) char g_signalStack[64 * 1024];

void writeAll(int fd, const char *data, size_t size)
{
    while (size > 0) {
        const ssize_t written = ::write(fd, data, size);
        if (written < 0 && errno == EINTR) continue;
        if (written <= 0) return;
        data += written;
        size -= size_t(written);
    }
}

char *appendNumber(char *out, unsigned long long number)
{
    char digits[20];
    unsigned count = 0;
    do {
        digits[count++] = char('0' + number % 10);
        number /= 10;
    } while (number);
    while (count) *out++ = digits[--count];
    return out;
}

void writeCrashLog(const char *reason)
{
    if (!g_prefix[0] || g_writing) return;
    g_writing = 1;
    char path[1100];
    char *out = path;
    for (const char *in = g_prefix; *in; ++in) *out++ = *in;
    timespec now{};
    ::clock_gettime(CLOCK_REALTIME, &now);
    out = appendNumber(out, static_cast<unsigned long long>(now.tv_sec));
    *out++ = '-';
    out = appendNumber(out, static_cast<unsigned long long>(::getpid()));
    for (const char *in = ".log"; *in; ++in) *out++ = *in;
    *out = '\0';
    const int fd = ::open(path, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW, 0600);
    if (fd < 0) return;
    size_t reasonSize = 0;
    while (reason[reasonSize]) ++reasonSize;
    writeAll(fd, reason, reasonSize);
    writeAll(fd, "\n", 1);

    const unsigned total = g_count.load(std::memory_order_acquire);
    const unsigned count = total < kRingSize ? total : kRingSize;
    char copy[kLineBytes];
    for (unsigned i = total - count; i < total; ++i) {
        const Line &line = g_lines[i % kRingSize];
        if (line.sequence.load(std::memory_order_acquire) != i + 1) continue;
        const unsigned size = line.size.load(std::memory_order_relaxed);
        for (unsigned j = 0; j < size; ++j) copy[j] = line.bytes[j].load(std::memory_order_relaxed);
        // A writer interrupted while replacing this slot leaves sequence
        // zero. Skip incomplete snapshots instead of blocking the crash.
        if (line.sequence.load(std::memory_order_acquire) != i + 1) continue;
        writeAll(fd, copy, size);
        writeAll(fd, "\n", 1);
    }
    ::close(fd);
}

void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    const QByteArray formatted = qFormatLogMessage(type, context, msg).toUtf8();
    {
        QMutexLocker locker(&g_ringMutex);
        const unsigned index = g_count.load(std::memory_order_relaxed);
        Line &line = g_lines[index % kRingSize];
        line.sequence.store(0, std::memory_order_release);
        const unsigned size = unsigned(qMin<qsizetype>(formatted.size(), kLineBytes));
        for (unsigned i = 0; i < size; ++i) line.bytes[i].store(formatted[i], std::memory_order_relaxed);
        line.size.store(size, std::memory_order_relaxed);
        line.sequence.store(index + 1, std::memory_order_release);
        g_count.store(index + 1, std::memory_order_release);
    }
    std::fprintf(stderr, "%s\n", formatted.constData());
    if (type == QtFatalMsg) std::abort();
}

[[noreturn]] void terminateHandler()
{
    writeCrashLog("std::terminate() called (unhandled exception)");
    std::abort();
}

void fatalSignalHandler(int sig, siginfo_t *info, void *)
{
    const char *name = "unknown fatal signal";
    switch (sig) {
    case SIGSEGV: name = "SIGSEGV"; break;
    case SIGABRT: name = "SIGABRT"; break;
    case SIGBUS: name = "SIGBUS"; break;
    case SIGILL: name = "SIGILL"; break;
    case SIGFPE: name = "SIGFPE"; break;
    }
    writeCrashLog(name);
    // Preserve the fault address for synchronous hardware faults: the
    // instruction re-faults under SIG_DFL on return. Raised signals need
    // to be queued again, because returning would otherwise resume.
    if (!info || info->si_code <= 0) ::kill(::getpid(), sig);
}
} // namespace

void install()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    const QByteArray prefix = (dir + QStringLiteral("/crash-")).toUtf8();
    if (prefix.size() < qsizetype(sizeof(g_prefix)))
        std::memcpy(g_prefix, prefix.constData(), size_t(prefix.size() + 1));
    qInstallMessageHandler(messageHandler);
    std::set_terminate(terminateHandler);
    stack_t stack{};
    stack.ss_sp = g_signalStack;
    stack.ss_size = sizeof(g_signalStack);
    if (::sigaltstack(&stack, nullptr) != 0) qWarning("Lane: could not install alternate crash signal stack");
    struct sigaction action{};
    action.sa_sigaction = fatalSignalHandler;
    action.sa_flags = SA_RESETHAND | SA_SIGINFO | SA_ONSTACK;
    ::sigemptyset(&action.sa_mask);
    for (int sig : {SIGSEGV, SIGABRT, SIGBUS, SIGILL, SIGFPE}) ::sigaction(sig, &action, nullptr);
}
} // namespace Lane::CrashLog
