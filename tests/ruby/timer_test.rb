class TimerTest < RedGemTest::Case
  FRAME = 0.25   # seconds; a quarter adds up without rounding

  def timer(seconds, repeats)
    @runs = 0
    Behaviour::Timer.new(seconds, repeats, proc { @runs += 1 })
  end

  def test_a_timer_runs_once_when_its_time_comes
    once = timer(FRAME * 3, false)
    refute once.__advance(FRAME)
    refute once.__advance(FRAME)
    assert once.__advance(FRAME), "done after running"
    assert_equal 1, @runs
  end

  def test_a_repeating_timer_keeps_going
    often = timer(FRAME * 2, true)
    5.times { refute often.__advance(FRAME) }
    assert_equal 2, @runs
  end

  def test_a_repeating_timer_catches_up_within_a_frame
    often = timer(FRAME, true)
    refute often.__advance(FRAME * 3)
    assert_equal 3, @runs
  end

  def test_a_tiny_interval_runs_a_few_times_a_frame_and_drops_the_rest
    tiny = timer(0.001, true)
    refute tiny.__advance(1.0)
    assert_equal Behaviour::Timer::MAX_RUNS_PER_FRAME, @runs
    refute tiny.__advance(0.0005)
    assert_equal Behaviour::Timer::MAX_RUNS_PER_FRAME, @runs, "what it missed was dropped"
  end

  def test_an_interval_of_zero_runs_once_a_frame
    every_frame = timer(0, true)
    3.times { refute every_frame.__advance(FRAME) }
    assert_equal 3, @runs
  end

  def test_a_cancelled_timer_is_done_and_never_runs
    cancelled = timer(FRAME, true)
    cancelled.cancel
    assert cancelled.__advance(1.0)
    assert_equal 0, @runs
  end

  def test_a_timer_cancelled_by_its_own_block_stops
    stopping = nil
    stopping = Behaviour::Timer.new(FRAME, true, proc { stopping.cancel })
    assert stopping.__advance(FRAME)
  end

  def test_smooth_progress_starts_and_ends_gently
    assert_in_delta 0.0, Behaviour::Tween.smooth(0.0)
    assert_in_delta 0.5, Behaviour::Tween.smooth(0.5)
    assert_in_delta 1.0, Behaviour::Tween.smooth(1.0)
    assert Behaviour::Tween.smooth(0.1) < 0.1, "slow to start"
    assert Behaviour::Tween.smooth(0.9) > 0.9, "slow to end"
  end

  def test_a_tween_ends_exactly_on_one
    seen = []
    tween = Behaviour::Tween.new(FRAME * 2, :linear, proc { |progress| seen << progress })
    refute tween.__advance(FRAME)
    assert tween.__advance(FRAME * 2)
    assert_in_delta 0.5, seen.first
    assert_equal 1.0, seen.last
  end
end
