# The texts a game shows its players, in their language. A game keeps them out of its scripts in
# translation files, one per language (locales/en.json, locales/pt.json...), each a JSON object
# of keys and texts; a text names the values put into it, which the call gives:
#
#   { "hud.coins": "Coins: {count}", "over.win": "You win!" }        # locales/en.json
#
#   UI.text(I18n.t("hud.coins", count: @coins), x: 20, y: 20)
#   I18n.locale = :pt                                                 # from now on, in Portuguese
#
# A text is looked for in the game's language in use, then in the game's English, then among the
# engine's own texts (the labels of GameMenu, such as game.menu.resume, which a game's files may
# also change); a key nothing has a text for shows as itself. The language starts as the player's.
module I18n
  # The text of `key` with its {placeholders} filled from `values`, each shown as its to_s.
  def self.t(key, **values) = __translate(key.to_s, values)

  # The language texts are shown in, such as :en.
  def self.locale = __locale.to_sym

  # Shows the texts in another language from now on (:pt, :pt_BR). A text looked up before keeps
  # its language: the labels of a GameMenu are looked up as it is added.
  def self.locale=(language)
    __set_locale(language.to_s)
  end

  # The languages the game has translation files for, such as [:en, :pt].
  def self.available_locales = __available_locales.map(&:to_sym)
end
