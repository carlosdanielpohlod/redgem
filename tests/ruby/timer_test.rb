# How a tween's progress is shaped. (What timers do as frames go by is tested with a game running:
# tests/integration/gameplay_api_test.cpp.)
class TimerTest < RedGemTest::Case
  def test_smooth_progress_starts_and_ends_gently
    assert_in_delta 0.0, Behaviour::Tween.smooth(0.0)
    assert_in_delta 0.5, Behaviour::Tween.smooth(0.5)
    assert_in_delta 1.0, Behaviour::Tween.smooth(1.0)
    assert Behaviour::Tween.smooth(0.1) < 0.1, "slow to start"
    assert Behaviour::Tween.smooth(0.9) > 0.9, "slow to end"
  end
end
