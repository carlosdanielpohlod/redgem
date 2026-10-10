# Kinematic platform that rises at a constant speed.
class Lift < Behaviour
  SPEED = 1.0

  def fixed_update(dt)
    transform.translate(Vector3.up * SPEED * dt)
  end
end
