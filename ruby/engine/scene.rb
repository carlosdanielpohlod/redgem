# Ruby side of the scene: what the engine's own Scene methods (find, find_all, objects...) are
# built on for games.
module Scene
  # Like find, for an object the game cannot work without: a missing one is an error, not a nil.
  # `wanted_by` names who asked, for the message.
  def self.find!(name, wanted_by: nil)
    find(name) || raise(ArgumentError, [wanted_by, "the scene has no object named #{name.inspect}"].compact.join(": "))
  end

  # Copies an object (usually a `prefab = true` template) into the running game, with its physics
  # and scripts. position: Vector3, rotation: Euler degrees (Vector3); both default to the template's.
  # physics: false leaves the copy without bodies: something to look at that nothing bumps into.
  def self.spawn(template, position: nil, rotation: nil, physics: true)
    __spawn(template, position, rotation, physics)
  end
end
