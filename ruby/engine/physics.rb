# Questions to the physics world: what a ray hits, what a shape moving along a line touches first,
# and what a shape put somewhere overlaps. Each of them takes
#   layers: the collision layers to look at, a name or a list of names (every layer when nil)
#   ignore: an object, or a list of objects, to see through (usually the one asking)
# and answers with hits, each { object:, point:, normal:, distance: }; lists come nearest first,
# each object once. Triggers are never hit.
#   hit = Physics.raycast(transform.position, Vector3.down, 2.0, layers: [:default])
#   near = Physics.overlap_sphere(transform.position, 3.0, layers: :enemy).map { |hit| hit[:object] }
module Physics
  RAY_LENGTH = 1000.0 # meters: how far a ray given no length looks

  # `skip` is an object the ray sees through, as those of `ignore:` are.
  def self.raycast(origin, direction, max_distance = RAY_LENGTH, skip = nil, layers: nil, ignore: nil)
    __raycast(origin, direction, max_distance, __objects([skip, ignore]), __layer_names(layers))
  end

  def self.raycast_all(origin, direction, max_distance = RAY_LENGTH, layers: nil, ignore: nil)
    __raycast_all(origin, direction, max_distance, __objects([ignore]), __layer_names(layers))
  end

  def self.sphere_cast(from, to, radius, layers: nil, ignore: nil)
    __cast(:sphere, [radius, 0, 0], nil, from, to, __objects([ignore]), __layer_names(layers))
  end

  # size is the box's whole width, height and depth; rotation in degrees, as transform.rotation.
  def self.box_cast(from, to, size, rotation: nil, layers: nil, ignore: nil)
    __cast(:box, size, rotation, from, to, __objects([ignore]), __layer_names(layers))
  end

  # An upright capsule (unless rotated): height is from end to end, at least twice the radius.
  def self.capsule_cast(from, to, radius, height, rotation: nil, layers: nil, ignore: nil)
    __cast(:capsule, [radius, height, 0], rotation, from, to, __objects([ignore]), __layer_names(layers))
  end

  def self.overlap_sphere(center, radius, layers: nil, ignore: nil)
    __overlap(:sphere, [radius, 0, 0], nil, center, __objects([ignore]), __layer_names(layers))
  end

  def self.overlap_box(center, size, rotation: nil, layers: nil, ignore: nil)
    __overlap(:box, size, rotation, center, __objects([ignore]), __layer_names(layers))
  end

  def self.overlap_capsule(center, radius, height, rotation: nil, layers: nil, ignore: nil)
    __overlap(:capsule, [radius, height, 0], rotation, center, __objects([ignore]), __layer_names(layers))
  end

  # Objects given alone or in lists, as one list.
  def self.__objects(given) = given.flat_map { |each| each.is_a?(Array) ? each : [each] }.compact

  def self.__layer_names(layers)
    return nil if layers.nil?

    layers.is_a?(Array) ? layers : [layers]
  end
end
