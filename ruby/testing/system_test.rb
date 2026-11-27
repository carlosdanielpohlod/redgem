# DSL for system tests: drives the real game frame by frame with simulated input.
#
#   SystemTest.define do |t|
#     t.hold(:d, frames: 30)
#     t.wait_until(frames: 120) { Scene.find("Coin").nil? }
#     t.assert("coin collected") { Scene.find("Coin").nil? }
#     t.screenshot("coin_collected")
#   end
#
# The block runs inside a Fiber; every wait yields one frame back to the engine.
class SystemTest
  include RedGemTest::Assertions

  DEFAULT_WAIT_UNTIL_FRAMES = 600
  MOST_DIFFERENT_SHARE = 0.01   # of a picture's pixels that may differ from its baseline
  PERCENT = 100
  MIN_FRAME_RATE = 4            # frames a second: a frame is a quarter of a second at most (kMaxFrameSeconds)

  attr_reader :failure

  def self.define(&block)
    $__redgem_system_test = new(block)
  end

  def self.current = $__redgem_system_test

  def initialize(block)
    @fiber = Fiber.new { block.call(self) }
    @failure = nil
    @finished = false
  end

  # Called by the engine once per frame. Returns :running, :waiting (a frame of a wait in real
  # seconds: the engine paces it and does not count it), :passed or :failed.
  def __tick
    return result if @finished

    @fiber.resume
    @finished = !@fiber.alive?
    return result if @finished

    @waiting_real_time ? :waiting : :running
  rescue Exception => e # rubocop:disable Lint/RescueException
    fail_with(e)
  end

  def wait(frames = 1)
    frames.times { Fiber.yield }
  end

  def wait_until(frames: DEFAULT_WAIT_UNTIL_FRAMES, message: "condition not met")
    frames.times do
      return if yield

      wait
    end
    raise RedGemTest::Failure, "#{message} within #{frames} frames"
  end

  # Waits until the block is true, or fails after that many real seconds: for what another
  # program does (a game that joins, a server that restarts). The frames run at the game's pace
  # and do not count toward the test's limit of frames.
  def wait_for(seconds:, message: "condition not met")
    raise ArgumentError, "wait_for takes seconds: a number" unless seconds.is_a?(Numeric)

    deadline = Time.now.to_f + seconds
    until yield
      raise RedGemTest::Failure, "#{message} within #{seconds} s" if Time.now.to_f >= deadline

      real_time_frame
    end
  end

  # Block form reads well in tests: t.assert("coin collected") { ... }.
  # Without a block it behaves like the regular assertion.
  def assert(*args, &block)
    return super(*args) unless block

    raise RedGemTest::Failure, args.first || "assertion failed" unless block.call
  end

  # Saves a picture of the next frame the game draws, as <name>.png in the screenshots folder.
  def screenshot(name)
    TestScreen.capture(name.to_s)
    wait
  end

  # Takes a picture of the next frame (as screenshot does) and fails unless it is close to a
  # baseline picture of the game's folder: at most `most` of its pixels (a share) differ by more
  # than a graphics card's rounding. REDGEM_UPDATE_BASELINES=1 writes the picture as the baseline.
  # Returns the share that differs.
  def match_baseline(name, baseline: "baselines/#{name}.png", most: MOST_DIFFERENT_SHARE)
    screenshot(name)
    share = TestScreen.difference(name.to_s, baseline)
    raise RedGemTest::Failure, "#{name}: #{(share * PERCENT).round(2)}% of it differs from #{baseline}" if share > most

    share
  end

  def log(message) = Debug.log("[test] #{message}")

  # Plays the game from the next frame as a window drawing `fps` frames a second would: each frame
  # that much game time, with as many fixed steps as it holds (none, one or more), and the bodies
  # drawn between their steps. The same every run. nil goes back to a fixed step a frame.
  def frame_rate=(fps)
    unless fps.nil? || frame_rate_usable?(fps)
      raise ArgumentError, "frame_rate takes from #{MIN_FRAME_RATE} frames a second up, or nil, not #{fps.inspect}"
    end

    TestInput.frame_seconds = fps ? 1.0 / fps : 0.0
  end

  private

  # (Slower than MIN_FRAME_RATE, a frame would be longer than the engine lets one be.)
  def frame_rate_usable?(fps) = fps.is_a?(Numeric) && fps.to_f.finite? && fps >= MIN_FRAME_RATE

  def real_time_frame
    @waiting_real_time = true
    wait
  ensure
    @waiting_real_time = false
  end

  # The test is over: no key stays down for whatever runs next.
  def fail_with(error)
    @finished = true
    @failure = "#{error.class}: #{error.message}"
    TestInput.release_all
    :failed
  end

  def result
    @failure ? :failed : :passed
  end
end
