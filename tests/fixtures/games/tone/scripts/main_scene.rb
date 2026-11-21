# Plays a steady 440 Hz tone as music from the first frame: what a recording's sound is checked by.
class MainScene < SceneScript
  def setup
    Audio.music(:tone, volume: 1.0)
  end
end
