# Walks back and forth along the direction it faces. The legs are moved by the model's own walk
# animation (which plays by itself); this only carries the person along at the matching speed.
# Settings: speed (m/s; a rigged model carries the pace of its own legs as its `speed` property),
# distance (m before turning back), reversed (the model's face points along -forward).
class Pedestrian < Behaviour
  DEFAULT_SPEED = 1.1
  DEFAULT_DISTANCE = 12.0
  TURN_DEGREES_PER_SECOND = 140.0
  HALF_TURN = 180.0
  DEGREES_PER_RADIAN = 180.0 / Math::PI

  def start
    @position = transform.position
    forward = transform.forward
    @yaw = Math.atan2(-forward.x, -forward.z) * DEGREES_PER_RADIAN
    @face_sign = prop(:reversed, false) ? -1.0 : 1.0
    @speed = prop(:speed, DEFAULT_SPEED)
    @distance = prop(:distance, DEFAULT_DISTANCE)
    @walked = 0.0
    @turn_left = 0.0
  end

  def update(dt)
    @turn_left.positive? ? turn(dt) : walk(dt)
    transform.position = @position
    transform.rotation = Vector3.new(0, @yaw, 0)
  end

  private

  def walk(dt)
    step = @speed * dt
    radians = @yaw / DEGREES_PER_RADIAN
    facing = Vector3.new(-Math.sin(radians), 0, -Math.cos(radians)) * @face_sign
    @position += facing * step
    @walked += step
    return if @walked < @distance

    @walked = 0.0
    @turn_left = HALF_TURN
  end

  def turn(dt)
    step = [TURN_DEGREES_PER_SECOND * dt, @turn_left].min
    @yaw += step
    @turn_left -= step
  end
end
