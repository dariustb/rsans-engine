#ifndef INCLUDED_RSANS_STEP_RUNNER
#define INCLUDED_RSANS_STEP_RUNNER

#include <rsans_data.h>
#include <rsans_task.h>

#include <cstddef>
#include <deque>
#include <functional>
#include <memory>
#include <string>
#include <thread>

// What a running step can report back to the UI. Safe to call from the
// worker thread: each call is forwarded to the UI thread.
struct StepContext {
    ProgressHandler onProgress;                       // 0.0 to 1.0
    std::function<void(const std::string&)> onLog;    // One log line
    std::function<void(const std::string&)> onStage;  // e.g. "Transcribing"
    const CancelFlag* cancel = nullptr;
};

// Runs one pipeline step at a time on a worker thread so the UI stays
// responsive. Every change to the runner's state happens through `post`,
// which must run the task on the UI thread (e.g. ScreenInteractive::Post),
// so the UI can read the state without locking.
class StepRunner {
  public:
    using Post = std::function<void(std::function<void()>)>;
    using Job = std::function<ProjectData(const StepContext&)>;
    using OnSuccess = std::function<void(ProjectData)>;

    enum class State { Idle, Running, Succeeded, Failed, Cancelled };

    static constexpr std::size_t maxLogLines = 500;

    explicit StepRunner(Post post);

    // Cancels a running step and waits for it to stop. Tasks it posted after
    // this point must not run (the UI loop has ended by then).
    ~StepRunner();

    StepRunner(const StepRunner&) = delete;
    StepRunner& operator=(const StepRunner&) = delete;
    StepRunner(StepRunner&&) = delete;
    StepRunner& operator=(StepRunner&&) = delete;

    // Starts `job` on the worker thread. onSuccess runs on the UI thread with
    // its result. Returns false (and does nothing) if a step is still running.
    bool start(const std::string& name, Job job, OnSuccess onSuccess);

    // Asks the running step to stop. It finishes as Cancelled.
    void cancel();

    // State, read from the UI thread
    State state() const { return d_state; }
    bool isRunning() const { return d_state == State::Running; }
    bool cancelRequested() const { return d_cancel && d_cancel->isRequested(); }
    const std::string& name() const { return d_name; }
    const std::string& stage() const { return d_stage; }
    double progress() const { return d_progress; }
    const std::string& error() const { return d_error; }
    const std::deque<std::string>& log() const { return d_log; }

  private:
    Post d_post;
    std::thread d_worker;
    std::unique_ptr<CancelFlag> d_cancel;

    State d_state = State::Idle;
    std::string d_name;
    std::string d_stage;
    double d_progress = 0.0;
    std::string d_error;
    std::deque<std::string> d_log;

    void addLog(const std::string& line);
};

#endif
