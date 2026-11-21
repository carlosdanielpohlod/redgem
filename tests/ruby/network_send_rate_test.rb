# How often an owner's NetworkSync sends: game.json's "network": {"send_hz": N} (Network.send_hz),
# or the object's own send_hz setting. The machines themselves are tested in
# tests/integration/multiplayer_test.cpp.
class NetworkSendRateTest < RedGemTest::Case
  DEFAULT_SEND_HZ = 20
  FAST = 64
  SLOW = 5 # poses a second: few enough that the others are shown it further behind
  WAY_TOO_FAST = 1000

  def synced(settings = {})
    sync = NetworkSync.new
    sync.__configure(settings)
    sync
  end

  def test_a_game_that_says_nothing_sends_20_poses_a_second
    assert_equal DEFAULT_SEND_HZ, Network.send_hz
    assert_in_delta DEFAULT_SEND_HZ, synced.send_hz
    assert_in_delta 1.0 / DEFAULT_SEND_HZ, synced.send_interval
  end

  def test_an_object_may_send_more_often_than_the_game
    assert_in_delta FAST, synced(send_hz: FAST).send_hz
    assert_in_delta 1.0 / FAST, synced(send_hz: FAST).send_interval
  end

  def test_an_object_sends_within_the_limits
    assert_in_delta Network::MAX_SEND_HZ, synced(send_hz: WAY_TOO_FAST).send_hz
    assert_in_delta Network::MIN_SEND_HZ, synced(send_hz: 0).send_hz
  end

  def test_the_others_expect_its_poses_that_often
    assert_in_delta 1.0 / SLOW * NetworkSync::Snapshots::INTERVALS_BEHIND, shown_behind(synced(send_hz: SLOW))
    assert_in_delta 1.0 / DEFAULT_SEND_HZ * NetworkSync::Snapshots::INTERVALS_BEHIND, shown_behind(synced)
  end

  def shown_behind(sync)
    sync.watch_afresh
    sync.instance_variable_get(:@snapshots).delay
  end
end
