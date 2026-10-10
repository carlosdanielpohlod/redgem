# Camera that stays behind the target, following its heading.
# Blender props: target, distance, height, smoothing.
class ChaseCamera < Behaviour
  DEFAULT_TARGET = "Car"
  DEFAULT_DISTANCE = 10.0
  DEFAULT_HEIGHT = 4.0
  DEFAULT_SMOOTHING = 4.0
  LOOK_HEIGHT = 1.0
  MAX_BLEND = 1.0

  def start
    @target = find(prop(:target, DEFAULT_TARGET))
    @distance = prop(:distance, DEFAULT_DISTANCE)
    @height = prop(:height, DEFAULT_HEIGHT)
    @smoothing = prop(:smoothing, DEFAULT_SMOOTHING)
    follow(MAX_BLEND) if @target
  end

  def update(dt)
    follow([@smoothing * dt, MAX_BLEND].min) if @target&.alive?
  end

  private

  def follow(blend)
    heading = @target.transform.forward.flat.normalized
    desired = @target.transform.position - heading * @distance + Vector3.up * @height
    transform.position = transform.position.lerp(desired, blend)
    transform.look_at(@target.transform.position + Vector3.up * LOOK_HEIGHT)
  end
end
