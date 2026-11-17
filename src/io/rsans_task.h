#ifndef INCLUDED_RSANS_TASK
#define INCLUDED_RSANS_TASK

#include <atomic>
#include <functional>
#include <stdexcept>

// Receives how far a long-running step has got, from 0.0 to 1.0. Called on
// the thread running the step.
using ProgressHandler = std::function<void(double fraction)>;

// Lets one thread ask a step running on another thread to stop. The step
// checks it regularly and throws CancelledError once it has stopped.
class CancelFlag {
  private:
    std::atomic<bool> d_requested{false};

  public:
    void request() noexcept { d_requested.store(true); }
    bool isRequested() const noexcept { return d_requested.load(); }
};

// Thrown by a step that stopped because cancellation was requested. Derives
// from std::runtime_error so existing error handling still catches it.
class CancelledError : public std::runtime_error {
  public:
    using std::runtime_error::runtime_error;
};

// Throws CancelledError if cancel is set and has been requested
inline void throwIfCancelled(const CancelFlag* cancel, const char* message) {
    if (cancel != nullptr && cancel->isRequested()) {
        throw CancelledError(message);
    }
}

#endif
