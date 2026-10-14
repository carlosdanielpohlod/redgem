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

  # Called by the engine once per frame. Returns :running, :passed or :failed.
  def __tick
    return result if @finished

    @fiber.resume
    @finished = !@fiber.alive?
    @finished ? result : :running
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

  def log(message) = Debug.log("[test] #{message}")

  private

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
