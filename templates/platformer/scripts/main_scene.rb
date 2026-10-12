# Sets the level up when it loads. Added here: the menus (a title screen, a pause menu on Esc or P
# and an end screen), which are the engine's GameMenu. Its title is the game's, from game.json.
class MainScene < SceneScript
  def setup
    Camera.main.add_component(GameMenu)
  end
end
