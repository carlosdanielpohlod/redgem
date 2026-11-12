# Sets the level up when it loads: the engine's menus (GameMenu), which here start closed, so the
# game starts at once. Esc, P or a gamepad's Start pauses it, with settings and the controls page.
# The texts players read are in locales/en.json (see I18n).
class MainScene < SceneScript
  def setup
    Camera.main.add_component(GameMenu, start_open: false)
  end
end
