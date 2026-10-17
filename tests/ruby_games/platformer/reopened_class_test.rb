# A test may reopen one of the game's classes to add to it: `class Mover` loads scripts/mover.rb
# first, so it reopens the game's Mover instead of making a new, empty class.
class Mover
  alias untraced_start start

  def traced_start
    @started = true
    untraced_start
  end
end

class ReopenedClassTest < RedGemTest::Case
  def test_a_reopened_class_is_the_game_s_with_what_the_test_added
    assert Mover < Behaviour
    assert_in_delta 3.0, Mover::DEFAULT_DISTANCE
    assert Mover.method_defined?(:untraced_start)
    assert Mover.method_defined?(:traced_start)
  end
end
