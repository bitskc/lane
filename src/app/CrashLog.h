#pragma once

namespace Lane
{
namespace CrashLog
{

// Mirrors Qt messages to stderr and retains the last 200 lines, capped
// at 2048 UTF-8 bytes per line. Fatal handlers read only published,
// lock-free bytes and use POSIX I/O, even if a signal interrupts logging.
// Logs are AppDataLocation/crash-<epoch>-<pid>.log, mode 0600.
// The default signal disposition is restored before re-raising, so core
// dumps are not suppressed.
// In-flight lines may be omitted. The installing thread has an alternate
// signal stack, so its stack-overflow faults can still write the log.
// Call once after setting the application's name/organization metadata.
void install();

} // namespace CrashLog
} // namespace Lane
