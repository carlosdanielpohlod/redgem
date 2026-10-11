# The platformer's level is built in Blender, which attaches most behaviours through custom
# properties. Added here: the menus (a pause menu on Esc or P, and the screen shown on winning) and
# playing together over the network.
class MainScene < SceneScript
  def setup
    Camera.main.add_component(GameMenu, title: "Platformer", start_open: false)
    find!("GameManager").add_component(CoOp)
  end
end
