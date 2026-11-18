#include <rsans_audio_player.h>
#include <rsans_test_utils.h>
#include <rsans_ticker.h>

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <functional>
#include <thread>

namespace {

// Waits up to two seconds for `done`
bool waitFor(const std::function<bool()>& done) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (std::chrono::steady_clock::now() < deadline) {
        if (done()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return done();
}

} // namespace

TEST(AudioPlayerTest, StartsClosedAndRefusesMissingFile) {
    AudioPlayer player(AudioPlayer::Output::Silent);
    EXPECT_EQ(player.state(), AudioPlayer::State::Closed);
    EXPECT_EQ(player.play(), "");  // Nothing to play: does nothing
    EXPECT_EQ(player.state(), AudioPlayer::State::Closed);

    EXPECT_NE(player.open("/no/such/song.wav"), "");
    EXPECT_EQ(player.state(), AudioPlayer::State::Closed);
    EXPECT_EQ(player.path(), "");
    EXPECT_NE(player.open(""), "");
}

TEST(AudioPlayerTest, OpenedFileIsStoppedAtStart) {
    const std::string wav = writeTestWav("rsans_player_open.wav", 2, 44100, 1500);
    AudioPlayer player(AudioPlayer::Output::Silent);

    EXPECT_EQ(player.open(wav), "");
    EXPECT_EQ(player.state(), AudioPlayer::State::Stopped);
    EXPECT_EQ(player.path(), wav);
    EXPECT_EQ(player.positionMs(), 0);
    EXPECT_EQ(player.lengthMs(), 1500);

    player.close();
    EXPECT_EQ(player.state(), AudioPlayer::State::Closed);
    EXPECT_EQ(player.lengthMs(), 0);
}

TEST(AudioPlayerTest, PlayPauseResumeAndStop) {
    const std::string wav = writeTestWav("rsans_player_play.wav", 1, 22050, 3000);
    AudioPlayer player(AudioPlayer::Output::Silent);
    ASSERT_EQ(player.open(wav), "");

    // Play: the position moves
    ASSERT_EQ(player.play(), "");
    EXPECT_EQ(player.state(), AudioPlayer::State::Playing);
    ASSERT_TRUE(waitFor([&] { return player.positionMs() > 0; }));

    // Pause: it stays put
    player.pause();
    EXPECT_EQ(player.state(), AudioPlayer::State::Paused);
    const int paused = player.positionMs();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_EQ(player.positionMs(), paused);

    // Resume: from there
    ASSERT_EQ(player.play(), "");
    ASSERT_TRUE(waitFor([&] { return player.positionMs() > paused; }));

    // Stop: back to the start
    player.stop();
    EXPECT_EQ(player.state(), AudioPlayer::State::Stopped);
    EXPECT_EQ(player.positionMs(), 0);
}

TEST(AudioPlayerTest, StopsAtStartWhenSongEnds) {
    const std::string wav = writeTestWav("rsans_player_end.wav", 1, 8000, 150);
    AudioPlayer player(AudioPlayer::Output::Silent);
    ASSERT_EQ(player.open(wav), "");
    ASSERT_EQ(player.play(), "");

    EXPECT_TRUE(waitFor([&] { return player.state() == AudioPlayer::State::Stopped; }));
    EXPECT_EQ(player.positionMs(), 0);

    // And plays again from the start
    ASSERT_EQ(player.play(), "");
    EXPECT_EQ(player.state(), AudioPlayer::State::Playing);
}

TEST(TickerTest, PostsWhileRunningAndStopsPromptly) {
    std::atomic<int> posts{0};
    Ticker ticker([&posts](const std::function<void()>& task) { task(); ++posts; }, std::chrono::milliseconds(10));
    EXPECT_FALSE(ticker.isRunning());

    ticker.start();
    EXPECT_TRUE(ticker.isRunning());
    EXPECT_TRUE(waitFor([&] { return posts >= 3; }));

    const auto before = std::chrono::steady_clock::now();
    ticker.stop();
    EXPECT_LT(std::chrono::steady_clock::now() - before, std::chrono::milliseconds(500));
    EXPECT_FALSE(ticker.isRunning());
    const int stopped = posts;
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_EQ(posts, stopped);

    ticker.start();  // Can start again
    EXPECT_TRUE(waitFor([&] { return posts > stopped; }));
}

TEST(AudioPlayerTest, PlayFromStartsAtThatPoint) {
    const std::string wav = writeTestWav("rsans_player_from.wav", 2, 44100, 3000);
    AudioPlayer player(AudioPlayer::Output::Silent);
    EXPECT_EQ(player.playFrom(1000), "");  // Closed: does nothing
    EXPECT_EQ(player.state(), AudioPlayer::State::Closed);
    ASSERT_EQ(player.open(wav), "");

    // From stopped
    ASSERT_EQ(player.playFrom(2000), "");
    EXPECT_EQ(player.state(), AudioPlayer::State::Playing);
    EXPECT_GE(player.positionMs(), 2000);
    ASSERT_TRUE(waitFor([&] { return player.positionMs() > 2000; }));

    // While playing: jumps back
    ASSERT_EQ(player.playFrom(500), "");
    EXPECT_EQ(player.state(), AudioPlayer::State::Playing);
    EXPECT_GE(player.positionMs(), 500);
    EXPECT_LT(player.positionMs(), 2000);

    // From paused
    player.pause();
    ASSERT_EQ(player.playFrom(0), "");
    EXPECT_EQ(player.state(), AudioPlayer::State::Playing);
    EXPECT_LT(player.positionMs(), 500);
}

TEST(AudioPlayerTest, PlayFromPastEndIsClampedAndEnds) {
    const std::string wav = writeTestWav("rsans_player_past.wav", 1, 8000, 1000);
    AudioPlayer player(AudioPlayer::Output::Silent);
    ASSERT_EQ(player.open(wav), "");

    ASSERT_EQ(player.playFrom(60000), "");
    EXPECT_LE(player.positionMs(), 1000);
    EXPECT_TRUE(waitFor([&] { return player.state() == AudioPlayer::State::Stopped; }));
    EXPECT_EQ(player.positionMs(), 0);

    ASSERT_EQ(player.playFrom(-5), "");  // Before the start: from the start
    EXPECT_LT(player.positionMs(), 1000);
}
