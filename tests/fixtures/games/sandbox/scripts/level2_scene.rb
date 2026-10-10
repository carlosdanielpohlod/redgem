# Runs when the sandbox's second scene loads.
class Level2Scene < SceneScript
  def setup
    $level2_setup_in = Game.scene_name
  end
end
