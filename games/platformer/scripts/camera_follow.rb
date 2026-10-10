# Smooth third-person camera. Blender props: target (object name), height, distance, smoothing.
class CameraFollow < Behaviour
  DEFAULT_TARGET = "Player"
  DEFAULT_HEIGHT = 5.0
  DEFAULT_DISTANCE = 9.0
  DEFAULT_SMOOTHING = 5.0
  LOOK_HEIGHT = 1.0
  MAX_BLEND = 1.0

  def start
    @target = find(prop(:target, DEFAULT_TARGET))
    @offset = Vector3.new(0, prop(:height, DEFAULT_HEIGHT), prop(:distance, DEFAULT_DISTANCE))
    @smoothing = prop(:smoothing, DEFAULT_SMOOTHING)
    snap
  end

  def update(dt)
    return unless @target&.alive?

    blend = [@smoothing * dt, MAX_BLEND].min
    transform.position = transform.position.lerp(desired_position, blend)
    transform.look_at(@target.transform.position + Vector3.up * LOOK_HEIGHT)
  end

  private

  def desired_position = @target.transform.position + @offset

  def snap
    return unless @target

    transform.position = desired_position
    transform.look_at(@target.transform.position + Vector3.up * LOOK_HEIGHT)
  end
end
