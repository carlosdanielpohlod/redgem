# The platformer's level is built in Blender, which attaches most behaviours through custom
# properties. Added here: the menus (a pause menu on Esc or P, and the screen shown on winning) and
# playing together over the network.
class MainScene < SceneScript
  # Where games meet to be played over the internet ("relay.example.com", a machine running
  # `redgem relay`). With none, the game is played on a local network, or by the host's address.
  RELAY = nil

  def setup
    Camera.main.add_component(GameMenu, title: "Platformer", start_open: false, relay: RELAY)
    find!("GameManager").add_component(CoOp)
  end
end
