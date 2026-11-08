# The Audio module with no game to take sounds from: what it refuses, and that a sound or a track
# with no file gets no handle and plays nothing. (Handles, groups and fades of sounds that play:
# tests/ruby_games/showcase/audio_test.rb and tests/unit/sound_handles_test.cpp.)
class AudioTest < RedGemTest::Case
  NOT_A_SOUND = :nothing_of_that_name

  def test_a_sound_without_a_file_has_no_handle
    assert_nil Audio.play(NOT_A_SOUND, loop: true, group: :voice)
    assert_nil Audio.play_at(NOT_A_SOUND, [0, 0, 0], group: :music)
  end

  def test_the_levels_of_a_sound_that_does_not_play_change_nothing
    refute Audio.set(1, volume: 0.5)
    refute Audio.set(1, pitch: 2)
    refute Audio.playing?(1)
  end

  def test_an_unknown_group_is_refused
    error = assert_raises(ArgumentError) { Audio.play(NOT_A_SOUND, group: :ambience) }
    assert error.message.include?("unknown mix group :ambience"), error.message
    assert_raises(ArgumentError) { Audio.play_at(NOT_A_SOUND, [0, 0, 0], group: :master) }
  end

  def test_a_pitch_or_a_fade_out_of_range_is_refused
    assert_raises(ArgumentError) { Audio.play(NOT_A_SOUND, pitch: 9) }
    assert_raises(ArgumentError) { Audio.set(1, pitch: 0) }
    assert_raises(ArgumentError) { Audio.music(NOT_A_SOUND, fade: -1) }
    assert_raises(ArgumentError) { Audio.stop_music(fade: -1) }
  end

  def test_music_without_a_file_does_not_play_and_stopping_no_music_is_harmless
    refute Audio.music(NOT_A_SOUND, fade: 1)
    assert_nil Audio.stop_music(fade: 1)
  end
end
