# What the main camera does on its own, frame after frame: follow an object, orbit one under the
# player's mouse or stick, and shake on top of either (the natives, main, fov, screen_ray and
# world_to_screen, are in camera_bindings.cpp).
#   Camera.follow(hero, offset: Vector3.new(0, 3, 6), smoothing: 0.15)
#   Camera.orbit(statue, distance: 8, pitch: 25)
#   Camera.shake(0.5, duration: 0.4)
# The camera follows or orbits one target at a time: the newest call wins. A target that is
# destroyed is let go, and the camera stays where it was. The camera moves after the behaviours
# that were there before its first helper call: a behaviour added after that sees it where it is
# drawn this frame (to mark things on it with world_to_screen).
module Camera
  # Nothing a camera is given is further or longer than this (meters, seconds, degrees): a larger
  # number is a mistake, and would break the picture.
  LARGEST = 1.0e6
  # Meters the camera keeps from a wall it is pulled in front of.
  WALL_MARGIN = 0.2
  # Meters it keeps from what it looks at, however near a wall is: nearer, it could not tell which
  # way to look.
  NEAREST_SEAT = 0.3
  HALF = 0.5

  # Follows an object at an offset; see Camera::Follow for the options. Returns the Follow.
  def self.follow(target, **)
    __rig("Camera.follow").motion = Follow.new(__object(target, "Camera.follow"), **)
  end

  # Orbits an object or a point (a Vector3); see Camera::Orbit for the options. Returns the Orbit.
  def self.orbit(target, **)
    __rig("Camera.orbit").motion = Orbit.new(__orbited(target), **)
  end

  # The camera stops following or orbiting, and stays where it is.
  def self.stop_following
    __current_rig&.motion = nil
  end

  # Whether the camera follows or orbits something.
  def self.following? = !__current_rig&.motion.nil?

  # Shakes the camera: strength (0 to 1) adds to its trauma, which never goes past 1, and fades
  # out over duration seconds. Returns the trauma.
  def self.shake(strength, duration: Shake::DURATION)
    strength = __number(strength, "Camera.shake strength", 0.0, Shake::MOST_TRAUMA)
    __rig("Camera.shake").shake.add(strength, __number(duration, "Camera.shake duration", Shake::SHORTEST))
  end

  # How much the camera shakes now, 0 to 1: it moves by the square of it.
  def self.trauma = __current_rig&.shake&.trauma || 0.0

  def self.stop_shaking
    __current_rig&.shake&.stop
  end

  # The main camera's rig, put on it the first time a helper is asked for.
  def self.__rig(asker)
    camera = main
    raise ArgumentError, "#{asker}: the scene has no camera" if camera.nil?

    camera.get_component(Rig) || camera.add_component(Rig)
  end

  # The rig of the main camera, when it has one (a new scene's camera has none yet).
  def self.__current_rig = main&.get_component(Rig)

  def self.__object(target, asker)
    return target if __alive_object?(target)

    raise ArgumentError, "#{asker} needs an object that is still there, not #{target.inspect}"
  end

  def self.__orbited(target)
    target.is_a?(Vector3) ? __vector(target, "Camera.orbit target") : __object(target, "Camera.orbit")
  end

  # A number from low to high (at most LARGEST): an ArgumentError naming what it is for otherwise.
  def self.__number(value, what, low, high = LARGEST)
    return value.to_f if __number_within?(value, low, high)

    raise ArgumentError, "#{what} must be a number from #{low} to #{high}, not #{value.inspect}"
  end

  def self.__alive_object?(target) = target.is_a?(GameObject) && target.alive?

  def self.__number_within?(value, low, high) = __finite_number?(value) && value.between?(low, high)

  def self.__finite_number?(value) = value.is_a?(Numeric) && value.to_f.finite?

  # A Vector3 (or [x, y, z]) none of whose numbers is larger than LARGEST.
  def self.__vector(value, what)
    vector = value.is_a?(Array) ? Vector3.new(*value) : value
    return vector if __small_vector?(vector)

    raise ArgumentError, "#{what} must be a Vector3 no larger than #{LARGEST}, not #{value.inspect}"
  end

  def self.__small_vector?(vector) = vector.is_a?(Vector3) && vector.to_a.all? { |part| part.abs <= LARGEST }

  # A Range of numbers from low to high, both ends in it: an ArgumentError otherwise.
  def self.__range(value, what, low, high)
    return value.begin.to_f..value.end.to_f if __range_within?(value, low, high)

    raise ArgumentError, "#{what} must be a range within #{low}..#{high}, not #{value.inspect}"
  end

  def self.__range_within?(value, low, high)
    return false unless __closed_range?(value)

    __number_within?(value.begin, low, high) && __number_within?(value.end, low, high) && value.begin <= value.end
  end

  # A Range that holds its last value (1..5, not 1...5).
  def self.__closed_range?(value) = value.is_a?(Range) && !value.exclude_end?

  # A number held within a range.
  def self.__clamp(value, range) = value.clamp(range.first, range.last)

  # How much of the way to where it should be a smoothed value goes in dt seconds, when it closes
  # half of it every half_life seconds: as far in one long frame as in many short ones.
  def self.__share(half_life, dt) = half_life.zero? ? 1.0 : 1.0 - HALF**(dt / half_life)

  # Where the camera can be on the way from `from` to `to` without a wall between: `to`, or in
  # front of the first wall the way meets, though never nearer `from` than NEAREST_SEAT. The object
  # followed, with whatever it carries, is seen through.
  def self.__clear_of_walls(from, to, ignore)
    way = to - from
    length = way.length
    return to if length <= NEAREST_SEAT

    hit = Physics.raycast(from, way / length, length + WALL_MARGIN, ignore: __with_children(ignore))
    hit ? from + way * (__seat_before(hit[:distance], length) / length) : to
  end

  # How far from what it looks at the camera sits before a wall that far away.
  def self.__seat_before(wall, length) = (wall - WALL_MARGIN).clamp(NEAREST_SEAT, length)

  # An object and everything it carries (its children, theirs...): none for a point.
  def self.__with_children(object)
    return [] unless object.is_a?(GameObject)

    [object] + object.children.flat_map { |child| __with_children(child) }
  end

  # Turns the camera to look at a point, unless it is right there: it keeps its turn then.
  def self.__look(transform, point)
    transform.look_at(point) if (point - transform.position).length > Vector3::EPSILON
  end
end
