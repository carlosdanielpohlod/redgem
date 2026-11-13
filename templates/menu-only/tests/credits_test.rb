# Unit tests: plain Ruby with no game running, for code that needs none. CreditsScreen is the
# game's own (scripts/credits_screen.rb): it loads by name, as it does in the game.
class CreditsTest < RedGemTest::Case
  SECONDS = 2.0
  UNSCROLLED = 0.0   # a list at its first row

  def test_the_credits_roll_at_their_speed
    assert_in_delta CreditsScreen::ROLL_SPEED * SECONDS, CreditsScreen.rolled(UNSCROLLED, SECONDS)
  end

  def test_every_credit_has_a_role_and_a_name
    CreditsScreen::CREDITS.each do |role, name|
      assert role.respond_to?(:call), "#{role.inspect} is not a lookup of a text (I18n.t)"
      refute name.empty?, "#{role.call} has nobody"
    end
  end
end
