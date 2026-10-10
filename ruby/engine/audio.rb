# Sounds and music from the game's sounds/ folder: Audio.play(:jump) plays sounds/jump.wav (or .ogg/.mp3).
module Audio
  FULL_VOLUME = 1.0
  NORMAL_PITCH = 1.0
  DEFAULT_MUSIC_VOLUME = 0.7

  # Returns false when the sound file does not exist.
  def self.play(name, volume: FULL_VOLUME, pitch: NORMAL_PITCH)
    __play(name, volume.to_f, pitch.to_f)
  end

  def self.music(name, loop: true, volume: DEFAULT_MUSIC_VOLUME)
    __music(name, loop, volume.to_f)
  end
end
