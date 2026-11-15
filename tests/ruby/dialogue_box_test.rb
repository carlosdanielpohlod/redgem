# How fast a DialogueBox's line appears (dialogue_box.rb); the box itself is played in
# tests/system/platformer/dialogue_box_test.rb.
class DialogueBoxTest < RedGemTest::Case
  LETTERS = 20
  SPEED = 10.0
  HALF = 1.0

  def test_a_line_appears_at_its_speed
    assert_equal 0, DialogueBox.letters_shown(LETTERS, 0.0, SPEED)
    assert_equal 10, DialogueBox.letters_shown(LETTERS, HALF, SPEED)
  end

  def test_no_more_than_the_line_shows
    assert_equal LETTERS, DialogueBox.letters_shown(LETTERS, HALF * 10, SPEED)
  end

  def test_no_speed_shows_it_all_at_once
    assert_equal LETTERS, DialogueBox.letters_shown(LETTERS, 0.0, 0.0)
  end

  def test_before_any_conversation_there_is_no_line_left_to_show
    assert DialogueBox.new.whole?
  end
end
