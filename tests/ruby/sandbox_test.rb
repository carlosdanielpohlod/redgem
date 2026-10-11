# A game's scripts may be someone else's code: the Ruby they run in can compute and reach the
# engine, and has no way to the player's files, to other programs or to the network on its own.
class SandboxTest < RedGemTest::Case
  FORBIDDEN_CONSTANTS = %i[File IO Dir Socket TCPSocket UDPSocket BasicSocket FileTest Binding].freeze
  FORBIDDEN_METHODS = %i[eval system exec spawn fork open binding exit!].freeze
  CODE_AS_TEXT = "1 + 1".freeze
  HALF_MORE = 1.5

  def test_no_files_processes_or_sockets_exist
    FORBIDDEN_CONSTANTS.each do |name|
      assert !Object.const_defined?(name), "#{name} must not exist in a game's Ruby"
    end
  end

  def test_nothing_runs_text_as_code_or_starts_programs
    FORBIDDEN_METHODS.each do |name|
      assert !respond_to?(name, true), "#{name} must not exist in a game's Ruby"
    end
  end

  def test_backticks_run_nothing
    assert_raises(NotImplementedError) { __send__(:`, "echo hello") }
  end

  # (With a block these only change who `self` is; it is text they must not run.)
  def test_text_is_never_evaluated_as_code
    assert_raises(Exception) { instance_eval(CODE_AS_TEXT) }
    assert_raises(Exception) { Object.class_eval(CODE_AS_TEXT) }
  end

  def test_what_scripts_do_need_is_there
    assert_equal 3, [1, 2].sum
    assert_equal "1.50", format("%<number>.2f", number: HALF_MORE)
    assert_in_delta 1.0, Math.sin(Math::PI / 2)
    assert_equal :done, Fiber.new { :done }.resume
    assert_equal 2, Struct.new(:a).new(2).a
    assert respond_to?(:__send__)
  end
end
