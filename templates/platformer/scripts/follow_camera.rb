# Keeps the camera behind and above its target (custom property "target", default "Player").
class FollowCamera < Behaviour
  HEIGHT = 5.0
  DISTANCE = 9.0
  SMOOTHING = 5.0
  LOOK_HEIGHT = 1.0
  MAX_BLEND = 1.0

  def start
    @target = find(prop(:target, "Player"))
  end

  def update(dt)
    return unless @target&.alive?

    desired = @target.transform.position + Vector3.new(0, prop(:height, HEIGHT), prop(:distance, DISTANCE))
    transform.position = transform.position.lerp(desired, [SMOOTHING * dt, MAX_BLEND].min)
    transform.look_at(@target.transform.position + Vector3.up * LOOK_HEIGHT)
  end
end
