# Audio with the showcase's sounds (redgem-test --ruby ... --game games/showcase): a flat sound's
# handle stops it and changes its levels, flat and positional sounds share one id space, and the
# music crossfades without stopping what plays when the new track is missing.
class AudioTest < RedGemTest::Case
  SOUND = :engine
  MISSING = :nothing_of_that_name
  FADE = 1.0

  def setup
    Audio.stop_sources
    Audio.stop_music
  end

  def test_a_looping_sound_has_a_handle_that_stops_it
    hum = Audio.play(SOUND, loop: true, volume: 0.4)
    assert hum.is_a?(Integer), "expected a handle, got #{hum.inspect}"
    assert Audio.playing?(hum)
    assert Audio.stop(hum)
    refute Audio.playing?(hum)
    refute Audio.stop(hum)
  end

  def test_a_sounds_volume_and_pitch_change_while_it_plays
    alarm = Audio.play(SOUND, loop: true, group: :voice)
    assert Audio.set(alarm, volume: 0.25, pitch: 1.5)
    assert Audio.set(alarm, pitch: 0.5)
    Audio.stop(alarm)
    refute Audio.set(alarm, volume: 1)
  end

  def test_flat_and_positional_sounds_share_one_id_space
    flat = Audio.play(SOUND)
    placed = Audio.play_at(SOUND, [1, 0, 0], group: :music)
    assert placed > flat, "#{placed} after #{flat}"
    Audio.stop_sources
    refute Audio.playing?(flat)
    refute Audio.playing?(placed)
  end

  def test_the_music_crossfades_and_a_missing_track_leaves_it_playing
    assert Audio.music(SOUND)
    assert Audio.music(SOUND, fade: FADE, volume: 0.5)
    refute Audio.music(MISSING, fade: FADE)
    assert_nil Audio.stop_music(fade: FADE)
  end
end
