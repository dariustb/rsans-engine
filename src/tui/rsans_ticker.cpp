#include <rsans_ticker.h>

#include <utility>

Ticker::Ticker(Post post, std::chrono::milliseconds every) : d_post(std::move(post)), d_every(every) {}

Ticker::~Ticker() {
    stop();
}

void Ticker::start() {
    if (d_thread.joinable()) {
        return;
    }
    d_stopping = false;
    d_thread = std::thread([this] {
        std::unique_lock<std::mutex> lock(d_mutex);
        // Wakes early to stop; otherwise posts once per period
        while (!d_wake.wait_for(lock, d_every, [this] { return d_stopping; })) {
            d_post([] {});
        }
    });
}

void Ticker::stop() {
    if (!d_thread.joinable()) {
        return;
    }
    {
        const std::lock_guard<std::mutex> lock(d_mutex);
        d_stopping = true;
    }
    d_wake.notify_all();
    d_thread.join();
}
