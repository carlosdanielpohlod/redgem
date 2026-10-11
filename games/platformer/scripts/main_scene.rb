# The platformer's level is built in Blender, which attaches most behaviours through custom
# properties. The menus are added here: a pause menu (Esc or P) and the screen shown on winning.
class MainScene < SceneScript
  def setup
    Camera.main.add_component(GameMenu, title: "Platformer", start_open: false)
  end
end
