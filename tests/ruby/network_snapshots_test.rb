class NetworkSnapshotsTest < RedGemTest::Case
  Snapshots = NetworkSync::Snapshots
  SEND = 0.05             # seconds between poses, as an owner sends them
  STILL = Vector3.zero
  NEAR = 1e-4
  JUMPED = 100.0          # meters: far more than an object moves between two poses
  LATE = 1.0              # seconds without a pose: far past what an object goes on for
  UNEVEN = [0.01, 0.12, 0.02, 0.11, 0.03, 0.1].freeze   # seconds between arrivals that wander

  def setup
    @snapshots = Snapshots.new(SEND)
  end

  # Poses along x, one every SEND seconds: 0, 1, 2...
  def arrive_along_x(count)
    count.times do |step|
      @snapshots.advance(SEND) if step.positive?
      @snapshots.add(Vector3[step, 0, 0], STILL)
    end
  end

  def shown_x = @snapshots.pose[0].x

  def test_nothing_is_shown_before_a_pose_arrives
    assert_nil @snapshots.pose
  end

  def test_steady_poses_are_shown_a_moment_behind_blended_between_two
    arrive_along_x(3)
    assert_in_delta SEND * Snapshots::INTERVALS_BEHIND, @snapshots.delay
    # 0.1 s on the clock, shown 0.075 behind: halfway from the first pose to the second.
    assert_in_delta 0.5, shown_x
    @snapshots.advance(SEND / 2)
    assert_in_delta 1.0, shown_x
  end

  def test_a_late_pose_is_made_up_for_a_moment_then_waited_for
    arrive_along_x(3)
    @snapshots.advance(LATE)
    speed = 1.0 / SEND
    went_on_to = 2.0 + speed * Snapshots::MAX_EXTRAPOLATION
    assert_in_delta went_on_to, shown_x
    @snapshots.advance(LATE)
    assert_in_delta went_on_to, shown_x
  end

  def test_a_teleport_is_shown_at_once_never_slid_across
    arrive_along_x(3)
    @snapshots.advance(SEND)
    @snapshots.add(Vector3[JUMPED, 0, 0], STILL)
    assert_in_delta JUMPED, shown_x
  end

  def arrive_unevenly(snapshots)
    UNEVEN.each do |gap|
      snapshots.advance(gap)
      snapshots.add(STILL, STILL)
    end
  end

  def test_uneven_arrivals_lengthen_the_delay_and_steady_ones_shorten_it_again
    arrive_unevenly(@snapshots)
    wandering = @snapshots.delay
    assert wandering > SEND * Snapshots::INTERVALS_BEHIND, "expected a longer delay than #{wandering}"
    assert wandering <= Snapshots::MAX_DELAY
    arrive_along_x(100)
    assert_in_delta SEND * Snapshots::INTERVALS_BEHIND, @snapshots.delay, 0.01
  end

  def test_a_pause_of_the_owner_is_not_taken_for_its_pace
    arrive_along_x(3)
    @snapshots.advance(LATE)
    @snapshots.add(STILL, STILL)
    assert_in_delta SEND * Snapshots::INTERVALS_BEHIND, @snapshots.delay
  end

  def test_a_delay_given_is_kept_whatever_the_arrivals
    fixed = Snapshots.new(SEND, 0.5)
    arrive_unevenly(fixed)
    assert_in_delta 0.5, fixed.delay
  end

  def test_two_poses_in_one_frame_count_as_the_newer
    @snapshots.add(Vector3[0, 0, 0], STILL)
    @snapshots.advance(SEND)
    @snapshots.add(Vector3[1, 0, 0], STILL)
    @snapshots.add(Vector3[2, 0, 0], STILL)
    @snapshots.advance(@snapshots.delay)
    assert_in_delta 2.0, shown_x
  end

  def test_turns_are_blended_the_short_way_round
    @snapshots.add(STILL, Vector3[0, 350, 0])
    @snapshots.advance(SEND)
    @snapshots.add(STILL, Vector3[0, 10, 0])
    @snapshots.advance(@snapshots.delay - SEND / 2)
    assert_in_delta 360.0, @snapshots.pose[1].y, NEAR
  end
end
