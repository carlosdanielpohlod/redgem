# Spins the object around its vertical axis. Try changing the speed in the Inspector
# (custom property "speed") or write your own behaviour next to this file.
class Spinner < Behaviour
  DEFAULT_SPEED = 45.0 # degrees per second

  def update(dt)
    transform.rotate(Vector3.new(0, prop(:speed, DEFAULT_SPEED) * dt, 0))
  end
end
