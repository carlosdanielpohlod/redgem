# The platformer's level is built in Blender, which attaches most behaviours through custom
# properties. Added here: the menus (a pause menu on Esc or P, and the screen shown on winning) and
# playing together over the network.
class MainScene < SceneScript
  # Where games meet to be played over the internet ("relay.example.com", a machine running
  # `redgem-relay`). With none, the game is played on a local network, or by the host's address.
  RELAY = nil
  TITLE = "Platformer".freeze
  MULTIPLAYER_OPTION = 2   # where the pause menu offers it: after Restart

  def setup
    add_menu
    find!("GameManager").add_component(CoOp)
  end

  private

  # The engine's menu, with a screen of the game's own.
  def add_menu
    menu = Camera.main.add_component(GameMenu, title: TITLE, start_open: false)
    menu.screens[:multiplayer] = MultiplayerScreen.new(menu, title: TITLE, relay: RELAY)
    menu.screens[:pause].add_option("Multiplayer", index: MULTIPLAYER_OPTION) { menu.show(:multiplayer) }
  end
end
