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
    @finished = true
    @failure = "#{e.class}: #{e.message}"
    TestInput.release_all
    :failed
  end

  def wait(frames = 1)
    frames.times { Fiber.yield }
  end

  def hold(*keys, frames:)
    keys.each { |key| TestInput.set(key, true) }
    wait(frames)
    keys.each { |key| TestInput.set(key, false) }
  end

  def press(key) = hold(key, frames: 1)

  # Keeps keys down until the block is true (or fails after `frames`).
  def hold_until(*keys, frames: DEFAULT_WAIT_UNTIL_FRAMES, message: "condition not met", &condition)
    keys.each { |key| TestInput.set(key, true) }
    wait_until(frames: frames, message: message, &condition)
  ensure
    keys.each { |key| TestInput.set(key, false) }
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

  def screenshot(name)
    Game.screenshot(name)
    wait
  end

  def log(message) = Debug.log("[test] #{message}")

  private

  def result
    @failure ? :failed : :passed
  end
end
