# Sets the game up when it loads: the engine's menus (GameMenu) and what this game adds to them
# (MenuFlow). The game opens on its title screen: Play, Settings (the engine's settings screen, and
# from it the controls page), Credits and Quit. The texts players read are in locales/en.json (see
# I18n): a locales/pt.json with the same keys shows them in Portuguese.
class MainScene < SceneScript
  def setup
    Camera.main.add_component(GameMenu)
    Camera.main.add_component(MenuFlow)
  end
end
