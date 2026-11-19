#include <rsans_step_runner.h>

#include <exception>
#include <memory>
#include <string>
#include <utility>

StepRunner::StepRunner(Post post)
: d_post(std::move(post)) {}

StepRunner::~StepRunner() {
    if (d_cancel) {
        d_cancel->request();
    }
    if (d_worker.joinable()) {
        d_worker.join();
    }
}

void StepRunner::addLog(const std::string& line) {
    d_log.push_back(line);
    if (d_log.size() > maxLogLines) {
        d_log.pop_front();
    }
}

bool StepRunner::start(const std::string& name, Job job, OnSuccess onSuccess) {
    if (isRunning()) {
        return false;
    }
    // The previous step has finished (its result was posted), so this is quick
    if (d_worker.joinable()) {
        d_worker.join();
    }

    d_state = State::Running;
    d_name = name;
    d_stage.clear();
    d_progress = 0.0;
    d_error.clear();
    addLog("=== " + name + " ===");
    d_cancel = std::make_unique<CancelFlag>();

    StepContext context;
    context.onProgress = [this](double fraction) {
        d_post([this, fraction] { d_progress = fraction; });
    };
    context.onLog = [this](const std::string& line) {
        d_post([this, line] { addLog(line); });
    };
    context.onStage = [this](const std::string& stage) {
        d_post([this, stage] {
            d_stage = stage;
            addLog("--- " + stage);
        });
    };
    context.cancel = d_cancel.get();

    d_worker = std::thread([this, job = std::move(job), onSuccess = std::move(onSuccess),
                            context = std::move(context)] {
        try {
            ProjectData result = job(context);
            d_post([this, onSuccess, result = std::move(result)]() mutable {
                d_state = State::Succeeded;
                d_progress = 1.0;
                addLog(d_name + " finished");
                if (onSuccess) {
                    onSuccess(std::move(result));
                }
            });
        } catch (const CancelledError& e) {
            const std::string message = e.what();
            d_post([this, message] {
                d_state = State::Cancelled;
                addLog(message);
            });
        } catch (const std::exception& e) {
            const std::string message = e.what();
            d_post([this, message] {
                d_state = State::Failed;
                d_error = message;
                addLog("Error: " + message);
            });
        } catch (...) {
            d_post([this] {
                d_state = State::Failed;
                d_error = "unknown error";
                addLog("Error: unknown error");
            });
        }
    });
    return true;
}

void StepRunner::cancel() {
    if (isRunning() && d_cancel) {
        d_cancel->request();
        addLog("Cancelling...");
    }
}
