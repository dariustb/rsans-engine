#include <rsans_process.h>

#ifdef _WIN32
#error "runProcess is POSIX only (uses posix_spawn, poll and signals)"
#endif

#include <array>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

extern char** environ;

namespace {

// How often the read loop wakes up to check for cancellation
constexpr int pollIntervalMs = 100;

// Closes a file descriptor when it goes out of scope
class FdCloser {
  private:
    int d_fd;

  public:
    explicit FdCloser(int fd) : d_fd(fd) {}
    ~FdCloser() { close(); }
    FdCloser(const FdCloser&) = delete;
    FdCloser& operator=(const FdCloser&) = delete;
    FdCloser(FdCloser&&) = delete;
    FdCloser& operator=(FdCloser&&) = delete;

    int get() const { return d_fd; }
    void close() {
        if (d_fd >= 0) {
            ::close(d_fd);
            d_fd = -1;
        }
    }
};

// Splits incoming bytes into lines and passes each non-empty line on
class LineSplitter {
  private:
    const ProcessOutputHandler& d_onLine;
    std::string d_line;

  public:
    explicit LineSplitter(const ProcessOutputHandler& onLine) : d_onLine(onLine) {}

    void add(const char* data, std::size_t size) {
        for (std::size_t i = 0; i < size; ++i) {
            if (data[i] == '\n' || data[i] == '\r') {
                flush();
            } else {
                d_line += data[i];
            }
        }
    }

    void flush() {
        if (!d_line.empty() && d_onLine) {
            d_onLine(d_line);
        }
        d_line.clear();
    }
};

// Starts the process with stdout and stderr both writing to outputFd
pid_t spawnProcess(const std::vector<std::string>& args, int readFd, int outputFd) {
    std::vector<char*> argv;
    argv.reserve(args.size() + 1);
    for (const std::string& arg : args) {
        argv.push_back(const_cast<char*>(arg.c_str()));
    }
    argv.push_back(nullptr);

    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_addopen(&actions, STDIN_FILENO, "/dev/null", O_RDONLY, 0);
    posix_spawn_file_actions_adddup2(&actions, outputFd, STDOUT_FILENO);
    posix_spawn_file_actions_adddup2(&actions, outputFd, STDERR_FILENO);
    posix_spawn_file_actions_addclose(&actions, readFd);
    posix_spawn_file_actions_addclose(&actions, outputFd);

    pid_t pid = -1;
    const int error = posix_spawnp(&pid, argv[0], &actions, nullptr, argv.data(), environ);
    posix_spawn_file_actions_destroy(&actions);

    if (error != 0) {
        throw std::runtime_error("Failed to start " + args[0] + ": " + std::strerror(error));
    }
    return pid;
}

int waitForExit(pid_t pid) {
    int status = 0;
    while (waitpid(pid, &status, 0) == -1) {
        if (errno != EINTR) {
            return -1;
        }
    }
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

} // namespace

int runProcess(const std::vector<std::string>& args,
               const ProcessOutputHandler& onLine,
               const CancelFlag* cancel,
               int killTimeoutMs) {
    if (args.empty()) {
        throw std::invalid_argument("runProcess: no program given");
    }
    throwIfCancelled(cancel, "Cancelled");

    std::array<int, 2> fds{};
    if (pipe(fds.data()) != 0) {
        throw std::runtime_error(std::string("Failed to create pipe: ") + std::strerror(errno));
    }
    FdCloser readEnd(fds[0]);
    FdCloser writeEnd(fds[1]);
    // Keep both ends out of any other process started meanwhile (e.g. by another thread)
    fcntl(readEnd.get(), F_SETFD, FD_CLOEXEC);
    fcntl(writeEnd.get(), F_SETFD, FD_CLOEXEC);

    const pid_t pid = spawnProcess(args, readEnd.get(), writeEnd.get());
    // Only the child writes now; closing our copy lets read() see EOF when it exits
    writeEnd.close();

    LineSplitter lines(onLine);
    std::array<char, 4096> buffer{};
    bool cancelling = false;
    bool killed = false;
    auto cancelledAt = std::chrono::steady_clock::now();

    try {
        while (true) {
            if (!cancelling && cancel != nullptr && cancel->isRequested()) {
                cancelling = true;
                cancelledAt = std::chrono::steady_clock::now();
                kill(pid, SIGTERM);
            }
            if (cancelling && !killed &&
                std::chrono::steady_clock::now() - cancelledAt >=
                    std::chrono::milliseconds(killTimeoutMs)) {
                killed = true;
                kill(pid, SIGKILL);
            }

            pollfd pfd{readEnd.get(), POLLIN, 0};
            const int ready = poll(&pfd, 1, pollIntervalMs);
            if (ready < 0) {
                if (errno == EINTR) {
                    continue;
                }
                break;
            }
            if (ready == 0) {
                continue;  // Timed out: loop round to check cancellation
            }

            const ssize_t bytesRead = read(readEnd.get(), buffer.data(), buffer.size());
            if (bytesRead > 0) {
                lines.add(buffer.data(), static_cast<std::size_t>(bytesRead));
            } else if (bytesRead == 0 || errno != EINTR) {
                break;  // EOF: the process closed its output (normally by exiting)
            }
        }
        lines.flush();
    } catch (...) {
        // onLine threw: stop the process rather than leave it running
        kill(pid, SIGKILL);
        waitForExit(pid);
        throw;
    }

    const int exitCode = waitForExit(pid);
    if (cancelling) {
        throw CancelledError("Cancelled");
    }
    return exitCode;
}
