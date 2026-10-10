class SystemTestDslTest < RedGemTest::Case
  def run_until_done(test, max_ticks = 100)
    statuses = []
    max_ticks.times do
      statuses << test.__tick
      break unless statuses.last == :running
    end
    statuses
  end

  def test_running_until_the_block_finishes
    steps = []
    test = SystemTest.new(proc { |t| steps << :a; t.wait(2); steps << :b })
    assert_equal [:running, :running, :passed], run_until_done(test)
    assert_equal %i[a b], steps
  end

  def test_failed_assertion_fails_the_test
    test = SystemTest.new(proc { |t| t.wait; t.assert("must be true") { false } })
    assert_equal [:running, :failed], run_until_done(test)
    assert test.failure.include?("must be true")
  end

  def test_regular_assertions_also_work
    test = SystemTest.new(proc { |t| t.assert_equal(1, 2) })
    assert_equal [:failed], run_until_done(test)
  end

  def test_wait_until_passes_when_condition_becomes_true
    counter = 0
    test = SystemTest.new(proc { |t| t.wait_until(frames: 10) { (counter += 1) >= 3 } })
    assert_equal :passed, run_until_done(test).last
  end

  def test_wait_until_times_out
    test = SystemTest.new(proc { |t| t.wait_until(frames: 3, message: "never") { false } })
    assert_equal :failed, run_until_done(test).last
    assert test.failure.include?("never within 3 frames")
  end

  def test_ruby_errors_fail_the_test
    test = SystemTest.new(proc { |_t| raise "kaput" })
    assert_equal [:failed], run_until_done(test)
    assert test.failure.include?("kaput")
  end

  def test_hold_keeps_keys_down_for_the_given_frames
    test = SystemTest.new(proc { |t| t.hold(:a, :d, frames: 2) })
    assert_equal [:running, :running, :passed], run_until_done(test)
  end

  def test_hold_until_stops_when_the_condition_holds
    ticks = 0
    held = []
    test = SystemTest.new(proc { |t| t.hold_until(:w, frames: 10) { held << TestInput.down?(:w); (ticks += 1) >= 3 } })
    statuses = run_until_done(test)
    assert_equal :passed, statuses.last
    assert_equal 3, statuses.size
    assert_equal [true, true, true], held
    refute TestInput.down?(:w), "key released after success"
  end

  def test_hold_until_fails_after_its_frame_budget
    test = SystemTest.new(proc { |t| t.hold_until(:w, frames: 2, message: "stuck") { false } })
    assert_equal :failed, run_until_done(test).last
    assert test.failure.include?("stuck within 2 frames")
    refute TestInput.down?(:w), "key released after failure"
  end

  def test_hold_releases_its_keys
    test = SystemTest.new(proc { |t| t.hold(:a, frames: 1) })
    run_until_done(test)
    refute TestInput.down?(:a)
  end

  def test_finished_tests_keep_their_status
    test = SystemTest.new(proc { |_t| })
    assert_equal :passed, test.__tick
    assert_equal :passed, test.__tick
  end
end
