# Sounds and music from the game's sounds/ folder: Audio.play(:jump) plays sounds/jump.wav (or .ogg/.mp3).
#
#   hum = Audio.play(:engine_hum, loop: true, volume: 0.4)   # a handle, or nil when it does not play
#   Audio.set(hum, pitch: 1.5)                               # its levels change while it plays
#   Audio.stop(hum)
#   Audio.play(:line_042, group: :voice)                     # the player's voice volume applies
#   Audio.music(:night, fade: 2.0)                           # crossfades from the track playing
module Audio
  FULL_VOLUME = 1.0
  NORMAL_PITCH = 1.0
  DEFAULT_MUSIC_VOLUME = 0.7
  NO_FADE = 0.0
  # What a sound is mixed in, unless it says (group: :music, :effects or :voice): each group has
  # the player's volume of its own (Settings.music_volume...).
  DEFAULT_GROUP = :effects

  # A sound with no place in the world. Returns its handle (for stop, playing? and set), or nil
  # when the sound file does not exist or 64 sounds already play.
  def self.play(name, volume: FULL_VOLUME, pitch: NORMAL_PITCH, loop: false, group: DEFAULT_GROUP)
    __play(name, volume.to_f, pitch.to_f, loop ? true : false, group)
  end

  # Changes the volume or the pitch of a sound that plays (play's or play_at's); false once it
  # stopped.
  def self.set(id, volume: nil, pitch: nil)
    __set(id, volume&.to_f, pitch&.to_f)
  end

  # The new track fades in over `fade` seconds while the one playing fades out. False when the
  # file does not exist: the track playing goes on.
  def self.music(name, loop: true, volume: DEFAULT_MUSIC_VOLUME, fade: NO_FADE)
    __music(name, loop ? true : false, volume.to_f, fade.to_f)
  end

  def self.stop_music(fade: NO_FADE)
    __stop_music(fade.to_f)
  end
end
