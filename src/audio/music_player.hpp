#pragma once

// The game's music: one track at a time, and while a new one fades in the old one fades out
// (Audio.music(:night, fade: 2)). Two streams play at most, so a crossfade costs one more decoder.

#include <cstdint>
#include <optional>
#include <string>

#include "audio/mix_groups.hpp"

namespace redgem {

using MusicTrackId = std::uint64_t;
inline constexpr float kNoFade = 0.0f;  // seconds: the change is heard at once

// What plays the streams. Tracks never share an id.
class MusicDriver {
 public:
  virtual ~MusicDriver() = default;
  // Starts a stream already at `volume`; false leaves nothing behind.
  virtual bool start(MusicTrackId id, const std::string& name, bool loop, float volume) = 0;
  virtual void update(MusicTrackId id) = 0;  // feeds the stream
  virtual void volume(MusicTrackId id, float volume) = 0;
  virtual void pitch(MusicTrackId id, float pitch) = 0;
  virtual void stop(MusicTrackId id) = 0;
};

struct MusicRequest {
  bool loop = true;
  float volume = kFullVolume;
  float fade_seconds = kNoFade;  // how long the new track takes to come in, and the old one to go
};

class MusicPlayer {
 public:
  explicit MusicPlayer(MusicDriver& driver) : driver_(driver) {}
  ~MusicPlayer();
  MusicPlayer(const MusicPlayer&) = delete;
  MusicPlayer& operator=(const MusicPlayer&) = delete;

  // False when the track cannot play: the one playing goes on then.
  bool play(const std::string& name, const MusicRequest& request);
  void stop(float fade_seconds);
  // The playing track's own volume and pitch; the one fading out keeps its own.
  void set_volume(float volume);
  void set_pitch(float pitch);
  // The music group's volume, over both tracks.
  void set_group_volume(float volume);
  // Moves the fades on by `seconds` and feeds the streams; every frame.
  void update(float seconds);
  // The track playing (not the one fading out), if any.
  std::optional<MusicTrackId> current() const;

 private:
  struct Track {
    MusicTrackId id = 0;
    float volume = kFullVolume;  // its own
    float level = 0.0f;          // how far it faded in: 0 silent, 1 at its volume
    float rate = 0.0f;           // how much level it gains a second (less than 0 fading out)
  };
  static Track faded_in(MusicTrackId id, const MusicRequest& request);
  float heard_volume(const Track& track) const;
  bool faded_out() const;  // the track fading out is silent now
  void leave(float fade_seconds);
  void advance(Track& track, float seconds);
  void drop(std::optional<Track>& track);

  MusicDriver& driver_;
  std::optional<Track> playing_;
  std::optional<Track> leaving_;
  float group_volume_ = kFullVolume;
  MusicTrackId next_ = 1;
};

}  // namespace redgem
