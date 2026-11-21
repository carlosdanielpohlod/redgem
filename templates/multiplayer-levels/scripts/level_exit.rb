# A door to another level (custom property "to": the level's name, "main" for scene.glb, "level2"
# for scenes/level2.glb). A player walks up to it and presses E (the :interact action): the whole
# party goes there. In a game played together the use runs on the host (or the dedicated server),
# and Game.load_scene there takes every machine along (see the manual's Multiplayer page).
class LevelExit < Behaviour
  RANGE = 3.0   # meters from the door the prompt shows

  def start
    return if get_component(Interactable)

    game_object.add_component(Interactable, prompt: "exit.travel", range: prop(:range, RANGE))
  end

  # The level it goes to.
  def to = prop(:to).to_s

  def on_interact(_actor)
    Game.load_scene(to) unless Network.changing_level?
  end
end
