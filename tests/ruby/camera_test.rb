# The camera helpers' arithmetic: smoothing and shaking come out the same however long the frames
# are, and a shake is the same every run. (What they do to a scene's camera is tested with a game
# running: tests/integration/camera_helpers_test.cpp.)
class CameraTest < RedGemTest::Case
  SLOW = 1.0 / 30
  FAST = 1.0 / 120
  CLOSE = 1e-6

  # Stands in for a transform: a place the helpers move, along the world's axes.
  class Spot
    attr_accessor :position

    def initialize(position)
      @position = position
    end

    def look_at(_point); end
    def rotate(_turn); end
    def right = Vector3.right
    def up = Vector3.up
    def forward = Vector3.forward
  end

  # Stands in for an object followed or orbited.
  class Thing
    attr_reader :transform

    def initialize(position)
      @transform = Spot.new(position)
    end

    def alive? = true
  end

  # Where the camera ends up after `seconds` of frames `step` long, the target moving as it is told.
  def camera_after(motion, seconds, step, target = nil, velocity = Vector3.zero)
    camera = Spot.new(Vector3.zero)
    (seconds / step).round.times do
      target.transform.position += velocity * step if target
      motion.update(camera, step)
    end
    camera.position
  end

  def follow(target) = Camera::Follow.new(target, offset: [0, 3, 6], smoothing: 0.3, avoid_walls: false)

  def test_smoothing_closes_half_the_way_every_half_life
    assert_in_delta 0.5, Camera.__share(0.2, 0.2)
    assert_in_delta 0.75, Camera.__share(0.2, 0.4)
    assert_in_delta 1.0, Camera.__share(0.0, SLOW)
  end

  def test_following_a_still_target_ends_in_the_same_place_at_30_and_120_frames_a_second
    slow = camera_after(follow(Thing.new(Vector3[2, 0, 1])), 0.5, SLOW)
    fast = camera_after(follow(Thing.new(Vector3[2, 0, 1])), 0.5, FAST)
    assert_vector slow, fast, CLOSE
    refute slow.approx?(Vector3[2, 3, 7], 0.1), "half a second is not all the way"
  end

  def test_following_a_moving_target_lags_behind_it_alike_at_any_frame_rate
    slow_target = Thing.new(Vector3.zero)
    fast_target = Thing.new(Vector3.zero)
    slow = camera_after(follow(slow_target), 2.0, SLOW, slow_target, Vector3[5, 0, 0])
    fast = camera_after(follow(fast_target), 2.0, FAST, fast_target, Vector3[5, 0, 0])
    # (Within the hero's move in a frame: a longer frame shows it a little further along.)
    assert_vector slow, fast, 5.0 * SLOW
  end

  def test_an_orbit_turns_as_far_at_30_and_120_frames_a_second
    orbits = [SLOW, FAST].map do |step|
      orbit = Camera::Orbit.new(Vector3.zero, distance: 4, input: false, avoid_walls: false)
      camera_after(orbit, FAST, FAST) # (it starts where it is asked to be)
      orbit.turn(90, 30)
      camera_after(orbit, 0.5, step)
    end
    assert_vector orbits.first, orbits.last, CLOSE
  end

  def test_an_orbit_goes_around_from_behind_to_the_right_and_above
    assert_vector Vector3.back, Camera::Orbit.direction(0, 0)
    assert_vector Vector3.right, Camera::Orbit.direction(90, 0)
    assert_vector Vector3.up, Camera::Orbit.direction(0, 90)
  end

  def test_an_orbit_keeps_within_its_limits
    orbit = Camera::Orbit.new(Vector3.zero, pitch: 10, distances: 2..8, pitches: -20..40)
    orbit.turn(400, 100)
    orbit.zoom(50)
    assert_in_delta 400.0, orbit.yaw
    assert_in_delta 40.0, orbit.pitch
    assert_in_delta 8.0, orbit.distance
  end

  def shaken(step, seconds)
    shake = Camera::Shake.new
    shake.add(0.8, 1.0)
    (seconds / step).round.times { shake.advance(step) }
    shake
  end

  def test_a_shake_is_the_same_at_30_and_120_frames_a_second
    slow = shaken(SLOW, 0.5)
    fast = shaken(FAST, 0.5)
    assert_in_delta 0.4, slow.trauma
    assert_in_delta slow.trauma, fast.trauma, CLOSE
    slow.offset.zip(fast.offset).each { |a, b| assert_vector a, b, CLOSE }
  end

  def test_a_shake_is_the_same_every_time
    assert_equal shaken(FAST, 0.3).offset.map(&:to_a), shaken(FAST, 0.3).offset.map(&:to_a)
    refute Camera::Shake.noise(1, 0, 3.5) == Camera::Shake.noise(2, 0, 3.5), "another seed shakes another way"
  end

  def test_shake_noise_stays_within_one_and_changes_smoothly
    samples = (0..400).map { |i| Camera::Shake.noise(Camera::Shake::SEED, 4, i * 0.01) }
    assert(samples.all? { |sample| sample.abs <= 1.0 })
    assert((1...samples.size).all? { |i| (samples[i] - samples[i - 1]).abs < 0.05 })
    assert samples.max - samples.min > 0.5, "it does move"
  end

  def test_shakes_stack_up_to_full_trauma_and_fade_out_over_their_duration
    shake = Camera::Shake.new
    assert_in_delta 0.6, shake.add(0.6, 0.5)
    assert_in_delta 1.0, shake.add(0.6, 0.5)
    shake.advance(0.25)
    assert_in_delta 0.5, shake.trauma
    shake.advance(0.3)
    refute shake.active?
    shake.offset.each { |part| assert_vector Vector3.zero, part }
  end

  def test_numbers_that_would_break_the_picture_are_refused
    [Float::NAN, Float::INFINITY, 1e300, -1, "1", nil].each do |wrong|
      assert_raises(ArgumentError) { Camera.__number(wrong, "smoothing", 0.0) }
    end
    assert_raises(ArgumentError) { Camera.__vector([0, 1e30, 0], "offset") }
    assert_raises(ArgumentError) { Camera.__range(5..1, "distances", 0.0, 10.0) }
    assert_in_delta 2.5, Camera.__number(2.5, "smoothing", 0.0)
  end
end
