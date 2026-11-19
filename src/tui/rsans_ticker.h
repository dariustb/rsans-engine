#ifndef INCLUDED_RSANS_TICKER
#define INCLUDED_RSANS_TICKER

#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>

// Posts an empty task at a steady pace while running, so the UI redraws
// things that change by themselves (e.g. the playing position). Started and
// stopped from the UI thread.
class Ticker {
  public:
    using Post = std::function<void(std::function<void()>)>;

    Ticker(Post post, std::chrono::milliseconds every);
    ~Ticker();  // Stops it

    Ticker(const Ticker&) = delete;
    Ticker& operator=(const Ticker&) = delete;
    Ticker(Ticker&&) = delete;
    Ticker& operator=(Ticker&&) = delete;

    void start();  // Does nothing if already running
    void stop();   // Waits for the thread to end
    bool isRunning() const { return d_thread.joinable(); }

  private:
    Post d_post;
    std::chrono::milliseconds d_every;
    std::thread d_thread;
    std::mutex d_mutex;
    std::condition_variable d_wake;
    bool d_stopping = false;
};

#endif
