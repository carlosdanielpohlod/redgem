# The camera behind the hero (custom property "target", default "Player"): the engine's
# Camera.orbit. Drag with the right mouse button, or push a gamepad's right stick, to look around
# the hero; the mouse wheel comes closer or goes further. It stays in front of walls that would
# hide the hero. The hero walks the way it looks (see Player).
class OrbitCamera < Behaviour
  DEFAULT_TARGET = "Player".freeze
  DISTANCE = 7.0          # meters behind the hero
  PITCH = 15.0            # degrees above it
  HEIGHT = 1.0            # meters above the hero's middle it looks at
  DISTANCES = 3.0..12.0   # how near and far the wheel takes it

  # The orbit (Camera::Orbit): its yaw, pitch and distance, for scripts to read and change.
  attr_reader :orbit

  def start
    target = find!(prop(:target, DEFAULT_TARGET))
    @orbit = Camera.orbit(target, distance: prop(:distance, DISTANCE), pitch: PITCH, height: HEIGHT,
                                  distances: DISTANCES)
  end
end
