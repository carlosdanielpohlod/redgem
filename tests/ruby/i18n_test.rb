# I18n with no game around it: the engine's own texts, placeholders and the language in use. (A
# game's own translation files are tested in tests/integration/game_translations_test.cpp.)
class I18nTest < RedGemTest::Case
  def setup
    I18n.locale = :en
  end

  def test_the_engines_texts_are_there_for_every_game
    assert_equal "Resume", I18n.t("game.menu.resume")
    assert_equal "Play again", I18n.t("game.menu.play_again")
    assert_equal "Paused", I18n.t(:"game.menu.paused")
  end

  def test_a_key_with_no_text_shows_as_itself
    assert_equal "hud.nothing_here", I18n.t("hud.nothing_here")
  end

  def test_placeholders_are_filled_with_what_each_value_shows_as
    assert_equal "Ana got 3 coins", I18n.t("{name} got {count} coins", name: "Ana", count: 3)
    assert_equal "{name} waits", I18n.t("{name} waits", other: 1)
    assert_equal "no values", I18n.t("no values", unused: nil)
  end

  def test_the_locale_can_be_changed_while_the_game_runs
    assert_equal :en, I18n.locale
    I18n.locale = :pt
    assert_equal :pt, I18n.locale
    I18n.locale = "pt_BR"
    assert_equal :pt_BR, I18n.locale
  end

  def test_a_language_with_no_texts_falls_back_to_english
    I18n.locale = :xx
    assert_equal "Quit", I18n.t("game.menu.quit")
  end

  def test_a_locale_that_names_no_language_is_refused
    ["", "../en", "en.json", "a" * 40].each do |name|
      error = assert_raises(ArgumentError) { I18n.locale = name }
      assert error.message.include?("I18n.locale must be a language's name"), error.message
    end
    assert_equal :en, I18n.locale
  end

  def test_with_no_game_there_are_no_translation_files
    assert_equal [], I18n.available_locales
  end
end
