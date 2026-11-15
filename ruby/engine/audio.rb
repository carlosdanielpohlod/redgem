# Sounds and music from the game's sounds/ folder: Audio.play(:jump) plays sounds/jump.wav (or .ogg/.mp3).
#
#   hum = Audio.play(:engine_hum, loop: true, volume: 0.4)   # a handle, or nil when it does not play
#   Audio.set(hum, pitch: 1.5)                               # its levels change while it plays
#   Audio.stop(hum)
#   Audio.play(:line_042, group: :voice)                     # the player's voice volume applies
#   Audio.music(:night, fade: 2.0)                           # crossfades from the track playing
#   Audio.ducking(volume: 0.25)                              # the music goes lower under voices
module Audio
  FULL_VOLUME = 1.0
  NORMAL_PITCH = 1.0
  DEFAULT_MUSIC_VOLUME = 0.7
  NO_FADE = 0.0
  # What a sound is mixed in, unless it says (group: :music, :effects or :voice): each group has
  # the player's volume of its own (Settings.music_volume...).
  DEFAULT_GROUP = :effects
  # With 128 sounds playing, a new one takes the place of the weakest (the lowest priority, then the
  # quietest, then the oldest) when its priority (0 to 255) is at least that one's.
  DEFAULT_PRIORITY = 128
  # While a sound of the :voice group plays, the music group goes down to this share of its volume
  # over DUCK_ATTACK seconds, and comes back over DUCK_RELEASE seconds once no voice plays.
  DUCKED_VOLUME = 0.4
  DUCK_ATTACK = 0.15
  DUCK_RELEASE = 0.6
  NO_DUCKING = 1.0

  # A sound with no place in the world. Returns its handle (for stop, playing? and set), or nil
  # when the sound file does not exist or 128 sounds of a higher priority already play.
  def self.play(name, volume: FULL_VOLUME, pitch: NORMAL_PITCH, loop: false, group: DEFAULT_GROUP,
                priority: DEFAULT_PRIORITY)
    __play(name, volume.to_f, pitch.to_f, loop ? true : false, group, priority)
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

  # How the music (its tracks and the sounds of the :music group) goes down while someone speaks.
  # volume: NO_DUCKING turns it off. The game's ducking ends with it.
  def self.ducking(volume: DUCKED_VOLUME, attack: DUCK_ATTACK, release: DUCK_RELEASE)
    __ducking(volume.to_f, attack.to_f, release.to_f)
  end

  def self.stop_music(fade: NO_FADE)
    __stop_music(fade.to_f)
  end
end
