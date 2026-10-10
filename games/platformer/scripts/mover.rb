# Moves a kinematic platform back and forth.
# Blender props: axis ("x", "y" or "z", in Blender axes), distance (meters), speed (radians/s).
class Mover < Behaviour
  DEFAULT_DISTANCE = 3.0
  DEFAULT_SPEED = 1.0
  BLENDER_AXES = {
    "x" => Vector3.blender(1, 0, 0),
    "y" => Vector3.blender(0, 1, 0),
    "z" => Vector3.blender(0, 0, 1)
  }.freeze

  def start
    @origin = transform.position
    @axis = BLENDER_AXES.fetch(prop(:axis, "x")) { raise ArgumentError, "Mover axis must be x, y or z" }
    @distance = prop(:distance, DEFAULT_DISTANCE)
    @speed = prop(:speed, DEFAULT_SPEED)
    @time = 0.0
  end

  def fixed_update(dt)
    @time += dt
    transform.position = @origin + @axis * (Math.sin(@time * @speed) * @distance)
  end
end
