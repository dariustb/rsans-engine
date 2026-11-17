#include <rsans_process.h>
#include <rsans_task.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;

std::vector<std::string> runAndCollect(const std::vector<std::string>& args, int& exitCode) {
    std::vector<std::string> lines;
    exitCode = runProcess(args, [&](const std::string& line) { lines.push_back(line); });
    return lines;
}

bool contains(const std::vector<std::string>& lines, const std::string& line) {
    return std::find(lines.begin(), lines.end(), line) != lines.end();
}

// Requests cancellation from another thread after a delay, like a TUI would
class DelayedCancel {
  private:
    std::thread d_thread;

  public:
    DelayedCancel(CancelFlag& cancel, std::chrono::milliseconds delay)
    : d_thread([&cancel, delay] {
          std::this_thread::sleep_for(delay);
          cancel.request();
      }) {}
    ~DelayedCancel() { d_thread.join(); }
    DelayedCancel(const DelayedCancel&) = delete;
    DelayedCancel& operator=(const DelayedCancel&) = delete;
    DelayedCancel(DelayedCancel&&) = delete;
    DelayedCancel& operator=(DelayedCancel&&) = delete;
};

} // namespace

TEST(ProcessTest, RunProcessCapturesStdoutAndStderrGivenBoth) {
    // When
    int exitCode = -1;
    const std::vector<std::string> lines =
        runAndCollect({"sh", "-c", "echo to-stdout; echo to-stderr >&2"}, exitCode);

    // Then
    EXPECT_EQ(exitCode, 0);
    EXPECT_TRUE(contains(lines, "to-stdout"));
    EXPECT_TRUE(contains(lines, "to-stderr"));
}

TEST(ProcessTest, RunProcessSplitsLinesGivenCarriageReturns) {
    // When
    int exitCode = -1;
    const std::vector<std::string> lines =
        runAndCollect({"printf", "one\rtwo\r\nthree"}, exitCode);

    // Then: '\r' ends a line, empty lines are skipped, and the unterminated last line is kept
    EXPECT_EQ(lines, (std::vector<std::string>{"one", "two", "three"}));
}

TEST(ProcessTest, RunProcessPassesArgumentsUnchangedGivenSpacesAndQuotes) {
    // Given: characters that would need quoting if a shell were involved
    const std::string arg = "a b 'c' \"d\" $HOME; e|f";

    // When
    int exitCode = -1;
    const std::vector<std::string> lines = runAndCollect({"printf", "%s\n", arg}, exitCode);

    // Then
    ASSERT_EQ(lines.size(), 1);
    EXPECT_EQ(lines[0], arg);
}

TEST(ProcessTest, RunProcessReturnsExitCodeGivenFailingProgram) {
    int exitCode = -1;
    runAndCollect({"sh", "-c", "exit 3"}, exitCode);
    EXPECT_EQ(exitCode, 3);
}

TEST(ProcessTest, RunProcessReturnsMinusOneGivenProgramKilledBySignal) {
    int exitCode = 0;
    runAndCollect({"sh", "-c", "kill -9 $$"}, exitCode);
    EXPECT_EQ(exitCode, -1);
}

TEST(ProcessTest, RunProcessGivesEmptyStdinGivenProgramThatReadsIt) {
    // Given: cat would wait forever if stdin were the terminal
    int exitCode = -1;
    const std::vector<std::string> lines = runAndCollect({"cat"}, exitCode);

    // Then
    EXPECT_EQ(exitCode, 0);
    EXPECT_TRUE(lines.empty());
}

TEST(ProcessTest, RunProcessThrowsGivenNonExistentProgram) {
    EXPECT_THROW(runProcess({"/nonexistent/program"}), std::runtime_error);
}

TEST(ProcessTest, RunProcessThrowsGivenNoArguments) {
    EXPECT_THROW(runProcess({}), std::invalid_argument);
}

TEST(ProcessTest, RunProcessThrowsCancelledWithoutStartingGivenCancelAlreadyRequested) {
    // Given
    CancelFlag cancel;
    cancel.request();
    bool ran = false;

    // When / Then
    EXPECT_THROW(runProcess({"sh", "-c", "echo started"},
                            [&](const std::string&) { ran = true; }, &cancel),
                 CancelledError);
    EXPECT_FALSE(ran);
}

TEST(ProcessTest, RunProcessStopsProgramGivenCancelDuringRun) {
    // Given
    CancelFlag cancel;
    const auto start = Clock::now();

    // When
    {
        DelayedCancel canceller(cancel, std::chrono::milliseconds(100));
        EXPECT_THROW(runProcess({"sleep", "10"}, {}, &cancel), CancelledError);
    }

    // Then: stopped by SIGTERM, well before sleep would have finished
    EXPECT_LT(Clock::now() - start, std::chrono::seconds(2));
}

TEST(ProcessTest, RunProcessKillsProgramGivenItIgnoresSigterm) {
    // Given: sleep inherits an ignored SIGTERM, so only SIGKILL stops it
    CancelFlag cancel;
    const auto start = Clock::now();

    // When
    {
        DelayedCancel canceller(cancel, std::chrono::milliseconds(100));
        EXPECT_THROW(runProcess({"sh", "-c", "trap '' TERM; exec sleep 10"}, {}, &cancel, 300),
                     CancelledError);
    }

    // Then: about 100 ms + the 300 ms kill timeout
    EXPECT_LT(Clock::now() - start, std::chrono::seconds(2));
}

TEST(ProcessTest, RunProcessStopsProgramGivenHandlerThrows) {
    // Given
    const auto start = Clock::now();

    // When / Then: the exception comes through and the process doesn't keep running
    EXPECT_THROW(runProcess({"sh", "-c", "echo line; exec sleep 10"},
                            [](const std::string&) { throw std::logic_error("handler failed"); }),
                 std::logic_error);
    EXPECT_LT(Clock::now() - start, std::chrono::seconds(2));
}
