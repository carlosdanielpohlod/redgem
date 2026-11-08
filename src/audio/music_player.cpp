#include "audio/music_player.hpp"

#include <algorithm>
#include <initializer_list>

namespace redgem {
namespace {

constexpr float kSilentLevel = 0.0f;
constexpr float kFullLevel = 1.0f;

bool fades(float seconds) { return seconds > kNoFade; }

}  // namespace

MusicPlayer::~MusicPlayer() {
  drop(playing_);
  drop(leaving_);
}

bool MusicPlayer::play(const std::string& name, const MusicRequest& request) {
  const Track track = faded_in(next_, request);
  if (!driver_.start(track.id, name, request.loop, heard_volume(track))) return false;
  ++next_;
  leave(request.fade_seconds);
  playing_ = track;
  return true;
}

void MusicPlayer::stop(float fade_seconds) { leave(fade_seconds); }

void MusicPlayer::set_volume(float volume) {
  if (!playing_) return;
  playing_->volume = volume;
  driver_.volume(playing_->id, heard_volume(*playing_));
}

void MusicPlayer::set_pitch(float pitch) {
  if (playing_) driver_.pitch(playing_->id, pitch);
}

void MusicPlayer::set_group_volume(float volume) {
  group_volume_ = volume;
  for (const auto* track : {&playing_, &leaving_}) {
    if (*track) driver_.volume((*track)->id, heard_volume(**track));
  }
}

void MusicPlayer::update(float seconds) {
  if (playing_) advance(*playing_, seconds);
  if (leaving_) advance(*leaving_, seconds);
  if (faded_out()) drop(leaving_);
}

std::optional<MusicTrackId> MusicPlayer::current() const {
  if (!playing_) return std::nullopt;
  return playing_->id;
}

MusicPlayer::Track MusicPlayer::faded_in(MusicTrackId id, const MusicRequest& request) {
  Track track;
  track.id = id;
  track.volume = request.volume;
  const bool fading = fades(request.fade_seconds);
  track.level = fading ? kSilentLevel : kFullLevel;
  track.rate = fading ? kFullLevel / request.fade_seconds : 0.0f;
  return track;
}

// (A track replaced before it was heard at all is silent already: it goes at once.)
bool MusicPlayer::faded_out() const { return leaving_ && leaving_->level <= kSilentLevel; }

float MusicPlayer::heard_volume(const Track& track) const { return track.volume * track.level * group_volume_; }

// The playing track starts fading out (or stops, with no fade); one already fading out stops.
void MusicPlayer::leave(float fade_seconds) {
  drop(leaving_);
  if (!playing_) return;
  leaving_ = playing_;
  playing_.reset();
  if (fades(fade_seconds)) {
    leaving_->rate = -leaving_->level / fade_seconds;
  } else {
    drop(leaving_);
  }
}

void MusicPlayer::advance(Track& track, float seconds) {
  track.level = std::clamp(track.level + track.rate * seconds, kSilentLevel, kFullLevel);
  driver_.update(track.id);
  driver_.volume(track.id, heard_volume(track));
}

void MusicPlayer::drop(std::optional<Track>& track) {
  if (!track) return;
  driver_.stop(track->id);
  track.reset();
}

}  // namespace redgem
