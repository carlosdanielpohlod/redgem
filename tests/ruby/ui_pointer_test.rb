# What a button makes of the mouse, and where the letters of a typed text begin.
class UiPointerTest < RedGemTest::Case
  def test_a_button_the_pointer_is_not_over_is_neither_held_nor_clicked
    refute UI.held?(false)
    refute UI.clicked?(false)
  end

  def test_a_letters_later_bytes_are_inside_it
    assert UI.inside_letter?("aç", 2), "the second byte of ç"
    refute UI.inside_letter?("aç", 1), "the first byte of ç"
    refute UI.inside_letter?("ab", 1)
  end

  def test_the_first_byte_of_a_text_is_inside_nothing
    refute UI.inside_letter?("ç", 0)
  end
end
