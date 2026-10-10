# Minimal xUnit-style framework for Ruby tests that run inside the engine's mruby.
module RedGemTest
  class Failure < StandardError; end

  DEFAULT_DELTA = 0.001

  module Assertions
    def assert(condition, message = "assertion failed")
      raise Failure, message unless condition
    end

    def refute(condition, message = "expected falsy value")
      assert(!condition, message)
    end

    def assert_equal(expected, actual, message = nil)
      assert(expected == actual, message || "expected #{expected.inspect}, got #{actual.inspect}")
    end

    def assert_nil(actual, message = nil)
      assert(actual.nil?, message || "expected nil, got #{actual.inspect}")
    end

    def assert_in_delta(expected, actual, delta = DEFAULT_DELTA, message = nil)
      assert((expected - actual).abs <= delta, message || "expected #{expected} +/- #{delta}, got #{actual}")
    end

    def assert_vector(expected, actual, delta = DEFAULT_DELTA)
      assert(expected.approx?(actual, delta), "expected #{expected.inspect}, got #{actual.inspect}")
    end

    def assert_raises(klass = StandardError)
      yield
    rescue klass => e
      e
    else
      raise Failure, "expected #{klass} to be raised"
    end
  end

  class Case
    include Assertions

    def self.inherited(subclass)
      super
      RedGemTest.cases << subclass
    end

    def setup; end
  end

  def self.cases
    @cases ||= []
  end

  # Runs every test_* method of every Case subclass. Returns the number of failures.
  def self.run_all
    failures = 0
    total = 0
    cases.each do |klass|
      klass.instance_methods(false).select { |m| m.to_s.start_with?("test_") }.sort.each do |method|
        total += 1
        begin
          instance = klass.new
          instance.setup
          instance.__send__(method)
          Debug.log("PASS #{klass}##{method}")
        rescue Exception => e # rubocop:disable Lint/RescueException
          failures += 1
          Debug.error("FAIL #{klass}##{method}: #{e.class}: #{e.message}")
        end
      end
    end
    Debug.log("#{total} tests, #{failures} failures")
    failures
  end
end
