# Sounds and music from the game's sounds/ folder: Audio.play(:jump) plays sounds/jump.wav (or .ogg/.mp3).
#
#   hum = Audio.play(:engine_hum, loop: true, volume: 0.4)   # a handle, or nil when it does not play
#   Audio.set(hum, pitch: 1.5)                               # its levels change while it plays
#   Audio.stop(hum)
#   Audio.play(:line_042, group: :voice)                     # the player's voice volume applies
#   Audio.music(:night, fade: 2.0)                           # crossfades from the track playing
#   Audio.ducking(volume: 0.25)                              # the music goes lower under voices
#   Audio.move(car_hum, car.position, car.velocity)          # higher coming nearer (Doppler)
#   Audio.group(:cave, parent: :effects)                     # a group of the game's own
#   Audio.effects(:cave, low_pass: 2_000, reverb: 0.4)       # muffled, with an echo
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
  NO_REVERB = 0.0
  # A sound played with play_at behind one of the level's static bodies (a wall, a floor) is heard
  # at OCCLUDED_VOLUME of its volume, through a low-pass at OCCLUDED_LOW_PASS Hz, after a fade of
  # OCCLUSION_FADE seconds.
  OCCLUDED_VOLUME = 0.4
  OCCLUDED_LOW_PASS = 1000.0
  OCCLUSION_FADE = 0.2

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

  # A group of the game's own under another (:effects unless it says), heard at its volume times
  # the volumes over it: Audio.play(:drip, group: :cave). The same name again moves it or changes its
  # volume. A game has 13 groups of its own at most; they end with it.
  def self.group(name, parent: DEFAULT_GROUP, volume: FULL_VOLUME)
    __group(name, parent, volume.to_f)
  end

  # What a group does to its sounds and to the groups under it: low_pass (Hz, 20 to 20000, nil for
  # none) muffles them, reverb (0 to 1) sends that much of them to the group's reverb. Calling it
  # again replaces them; Audio.effects(:cave) takes them off.
  def self.effects(group, low_pass: nil, reverb: NO_REVERB)
    __effects(group, low_pass&.to_f, reverb.to_f)
  end

  # How a sound behind a wall is heard: volume: 1 and low_pass: nil turn it off. The game's
  # occlusion ends with it. A sound plays where no wall muffles it with play_at's occlusion: false.
  def self.occlusion(volume: OCCLUDED_VOLUME, low_pass: OCCLUDED_LOW_PASS, fade: OCCLUSION_FADE)
    __occlusion(volume.to_f, low_pass&.to_f, fade.to_f)
  end

  def self.stop_music(fade: NO_FADE)
    __stop_music(fade.to_f)
  end
end
