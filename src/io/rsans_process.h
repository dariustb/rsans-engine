#ifndef INCLUDED_RSANS_PROCESS
#define INCLUDED_RSANS_PROCESS

#include <rsans_task.h>

#include <functional>
#include <string>
#include <vector>

// Receives each line a process writes to stdout or stderr
using ProcessOutputHandler = std::function<void(const std::string& line)>;

// Runs a program directly (no shell, so arguments need no quoting) and waits
// for it to finish. args[0] is the program, found on PATH if it has no '/'.
//
// - stdout and stderr are captured and passed to onLine one line at a time;
//   '\r' also ends a line. stdin is /dev/null.
// - If cancel is requested, the process is sent SIGTERM, then SIGKILL if it
//   has not exited within killTimeoutMs, and CancelledError is thrown.
// - ownProcessGroup: start the process in a new process group and signal the
//   whole group, so programs it starts (e.g. a shell script's curl) are
//   stopped too. Off by default so a terminal's Ctrl+C still reaches it.
//
// Returns the exit code, or -1 if the process was ended by a signal.
// Throws std::runtime_error if the program cannot be started.
// POSIX only (Linux, macOS).
int runProcess(const std::vector<std::string>& args,
               const ProcessOutputHandler& onLine = {},
               const CancelFlag* cancel = nullptr,
               int killTimeoutMs = 3000,
               bool ownProcessGroup = false);

#endif
