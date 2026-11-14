# Ruby side of the scene: what the engine's own Scene methods (find, find_all, objects...) are
# built on for games.
module Scene
  # Like find, for an object the game cannot work without: a missing one is an error, not a nil.
  # `wanted_by` names who asked, for the message.
  def self.find!(name, wanted_by: nil)
    find(name) || raise(ArgumentError, [wanted_by, "the scene has no object named #{name.inspect}"].compact.join(": "))
  end

  # Copies an object (usually a `prefab = true` template) into the running game, with its physics
  # and scripts. position: Vector3, rotation: Euler degrees (Vector3); both are in the world and
  # default to the template's. physics: false leaves the copy without bodies: something to look at
  # that nothing bumps into. name: what the copy is called (Scene.find finds it by it); the
  # template's name by default. parent: an object the copy goes under, keeping its place in the
  # world (it then moves with it).
  def self.spawn(template, position: nil, rotation: nil, physics: true, name: nil, parent: nil)
    __spawn(template, position, rotation, physics, name, parent)
  end

  # Instantiates a rigid GLB named relative to prefabs/.
  def self.spawn_prefab(file, position: nil, rotation: nil, physics: true, name: nil, parent: nil)
    __spawn_prefab(file, position, rotation, physics, name, parent)
  end
end
