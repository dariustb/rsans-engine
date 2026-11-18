#ifndef INCLUDED_RSANS_AUDIO_PLAYER
#define INCLUDED_RSANS_AUDIO_PLAYER

#include <memory>
#include <string>

// Plays one audio file through the sound device, with play, pause and stop.
// Files are read with libsndfile, so it plays every format the project
// accepts; miniaudio sends the sound to the device, converting it to the
// device's sample rate and channels.
//
// Called from one thread (the UI's). The sound device is only opened when
// playing starts, so a computer without one can still open files.
class AudioPlayer {
  public:
    enum class State { Closed, Stopped, Playing, Paused };
    // Where the sound goes: the default sound device, or nowhere, at the
    // same pace (for tests, and computers without sound)
    enum class Output { Device, Silent };

    explicit AudioPlayer(Output output = Output::Device);
    ~AudioPlayer();

    AudioPlayer(const AudioPlayer&) = delete;
    AudioPlayer& operator=(const AudioPlayer&) = delete;
    AudioPlayer(AudioPlayer&&) = delete;
    AudioPlayer& operator=(AudioPlayer&&) = delete;

    // Opens a file, stopped at the start (closing any open one). Returns ""
    // or why it can't be played; on failure the player is Closed.
    std::string open(const std::string& path);
    void close();
    // The open file, or "" when Closed
    const std::string& path() const;

    // Starts or resumes playing. Returns "" or why the sound device couldn't
    // be started. Does nothing if Closed or already playing.
    std::string play();
    // Plays from a point in the file (clamped to it), whatever the state.
    // Returns "" or why it couldn't. Does nothing if Closed.
    std::string playFrom(int ms);
    void pause();  // Keeps the position
    void stop();   // Back to the start

    // Playing ends by itself at the end of the file: it's then Stopped, back
    // at the start (noticed here, so it's up to date when asked)
    State state();

    // Where playing is, and the file's length, in milliseconds
    int positionMs() const;
    int lengthMs() const;

  private:
    struct Impl;
    std::unique_ptr<Impl> d_impl;
};

#endif
