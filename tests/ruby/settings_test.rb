# The Settings module with no window to show the game in: what it reads and changes, what it
# refuses, and that nothing can be kept without a running game.
class SettingsTest < RedGemTest::Case
  # (Every test starts from what a player who chose nothing has.)
  def setup
    Settings.window_mode = :windowed
    Settings.quality = :ultra
    Settings.render_scale = 1.0
    Settings.vsync = true
    Settings.fps_cap = 60
  end

  def test_a_player_who_chose_nothing_gets_a_window_at_the_engines_pace
    assert_equal :windowed, Settings.window_mode
    refute Settings.fullscreen?
    assert_equal true, Settings.vsync
    assert_equal 60, Settings.fps_cap
    assert_in_delta 1.0, Settings.render_scale
    assert_equal :ultra, Settings.quality
  end

  def test_fullscreen_is_one_of_the_window_modes
    Settings.fullscreen = true
    assert_equal :fullscreen, Settings.window_mode
    Settings.window_mode = :borderless
    refute Settings.fullscreen?
    Settings.fullscreen = false
    assert_equal :windowed, Settings.window_mode
  end

  def test_the_resolution_is_held_to_what_a_window_can_be
    Settings.resolution = [1920, 1080]
    assert_equal [1920, 1080], Settings.resolution
    Settings.resolution = [10, 1080]
    assert_equal [320, 1080], Settings.resolution
  end

  def test_the_render_scale_is_held_from_half_to_twice
    Settings.render_scale = 0.75
    assert_in_delta 0.75, Settings.render_scale
    Settings.render_scale = 5
    assert_in_delta 2.0, Settings.render_scale
    Settings.render_scale = 0.1
    assert_in_delta 0.5, Settings.render_scale
  end

  def test_no_frame_cap_reads_as_nil
    Settings.fps_cap = 144
    assert_equal 144, Settings.fps_cap
    Settings.fps_cap = nil
    assert_nil Settings.fps_cap
  end

  def test_vsync_and_volume
    Settings.vsync = false
    refute Settings.vsync
    Settings.volume = 0.25
    assert_in_delta 0.25, Settings.volume
    Settings.volume = 3
    assert_in_delta 1.0, Settings.volume
  end

  def test_a_quality_is_one_of_the_presets
    Settings.quality = :medium
    assert_equal :medium, Settings.quality
    error = assert_raises(ArgumentError) { Settings.quality = :cinematic }
    assert error.message.include?("unknown quality :cinematic"), error.message
    assert_raises(ArgumentError) { Settings.window_mode = :maximized }
  end

  def test_quality_details_make_the_quality_custom_and_keep_the_others
    Settings.quality_details = { msaa: 2, bloom: 0.5 }
    assert_equal :custom, Settings.quality
    details = Settings.quality_details
    assert_equal [2, true], [details[:msaa], details[:antialias]]
    assert_in_delta 0.5, details[:bloom]
    assert_raises(ArgumentError) { Settings.quality_details = { ray_tracing: true } }
  end

  def test_with_no_running_game_nothing_is_kept_and_no_window_size_is_offered
    refute Settings.save
    assert_equal [], Settings.resolutions
  end
end
