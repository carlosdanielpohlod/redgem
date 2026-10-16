# The platformer's texts, from its locales folder (redgem-test --ruby ... --game games/platformer).
class PlatformerI18nTest < RedGemTest::Case
  def setup
    I18n.locale = :en
  end

  def test_the_game_has_its_texts_in_english
    assert_equal [:en], I18n.available_locales
    assert_equal "Coins 2/5", I18n.t("hud.coins", coins: 2, total: 5)
    assert_equal "New record: 12.3 s!", I18n.t("over.record", seconds: "12.3")
  end

  def test_a_language_the_game_lacks_shows_its_english_and_the_engines_texts
    I18n.locale = :pt
    assert_equal "You win!", I18n.t("over.win")
    assert_equal "Resume", I18n.t("game.menu.resume")
  end

  def test_a_message_says_how_many_other_players_there_are
    assert_equal "Room K7QM3P: 1 other player", I18n.t("multiplayer.one_other", place: "Room K7QM3P")
    assert_equal "Room K7QM3P: 2 other players", I18n.t("multiplayer.others", place: "Room K7QM3P", count: 2)
  end
end
