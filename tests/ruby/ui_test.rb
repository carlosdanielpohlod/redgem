class UiTest < RedGemTest::Case
  def test_shade_darkens_every_channel
    assert_equal [100, 50, 0], UI.shade([200, 100, 0], 0.5)
    assert_equal UI::COLORS[:white], UI.shade(:white, 1.0)
    assert_equal UI::COLORS[:black], UI.shade(:ruby, 0.0)
  end

  def test_a_held_button_is_darker
    assert_equal UI::COLORS[:ruby], UI.button_fill(:ruby, pressed: false)
    assert_equal UI.shade(:ruby, UI::PRESSED_SHADE), UI.button_fill(:ruby, pressed: true)
    assert_equal [10, 20, 30], UI.button_fill([10, 20, 30], pressed: false)
  end

  def test_label_size_is_a_share_of_the_height
    assert_equal 24, UI.label_size(58)
    assert_equal 12, UI.label_size(30)
    assert_equal 0, UI.label_size(0)
  end

  def test_centered_puts_the_middles_together
    assert_equal 120, UI.centered(100, 60, 20)
    assert_equal 130, UI.centered(100, 60, 0)
    assert_equal 103, UI.centered(100, 7, 0), "whole pixels"
  end

  def test_chop_takes_a_whole_letter
    assert_equal "ab", UI.chop("abc")
    assert_equal "a", UI.chop("aç")
    assert_equal "", UI.chop("ç")
    assert_equal "", UI.chop("")
  end

  def test_letters_counts_letters_not_bytes
    assert_equal 3, UI.letters("açb")
    assert_equal 0, UI.letters("")
  end
end
