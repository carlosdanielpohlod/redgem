# The engine's menus over a plain scene, for the pictures of how they look
# (tests/system_fixtures/menu_look).
class MainScene < SceneScript
  def setup
    Camera.main.add_component(GameMenu)
  end
end
