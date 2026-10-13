#pragma once

namespace Lane
{
namespace CrashLog
{

// Installs a Qt message handler that mirrors every log message to stderr
// (same as the default handler) and also keeps the last ~200 lines in a
// ring buffer, plus a std::terminate handler and fatal-signal handlers
// (SIGSEGV/SIGABRT/SIGBUS/SIGILL/SIGFPE) that flush that ring buffer to
// QStandardPaths::AppDataLocation/crash-<epoch>.log on an unhandled
// exception or fatal signal, replacing KCrash::initialize(). Signal
// handlers restore the default disposition and re-raise afterward, so a
// core dump (if enabled) is never suppressed.
//
// Call once, first thing in main(), before constructing anything else.
void install();

} // namespace CrashLog
} // namespace Lane
