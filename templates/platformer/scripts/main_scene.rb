# Sets the level up when it loads. Added here: the menus (a title screen, a pause menu on Esc or P
# and an end screen), which are the engine's GameMenu, and playing online (see Online). The menu's
# title is the game's, from game.json. The texts players read are in locales/en.json (see I18n):
# a locales/pt.json with the same keys shows them in Portuguese.
class MainScene < SceneScript
  def setup
    Camera.main.add_component(GameMenu)
    Camera.main.add_component(Online)
  end
end
