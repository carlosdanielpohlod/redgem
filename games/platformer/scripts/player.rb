# Third-person platformer hero: walks with WASD/arrows, jumps with Space.
# Blender props: speed, jump_speed.
class Player < Behaviour
  SPEED = 6.0
  JUMP_SPEED = 9.0
  FALL_LIMIT = -10.0
  DEGREES_PER_RADIAN = 180.0 / Math::PI

  attr_reader :respawns

  def start
    @spawn = transform.position
    @respawns = 0
  end

  def update(_dt)
    direction = Vector3.new(Input.axis(:horizontal), 0, -Input.axis(:vertical)).normalized
    character.move(direction * prop(:speed, SPEED))
    character.jump(prop(:jump_speed, JUMP_SPEED)) if Input.action_pressed?(:jump) && character.grounded?
    face(direction) unless direction == Vector3.zero
    respawn if transform.position.y < FALL_LIMIT
  end

  def respawn
    transform.position = @spawn
    @respawns += 1
  end

  private

  # Turns the hero so its forward (-Z) points along the movement direction.
  def face(direction)
    yaw = Math.atan2(-direction.x, -direction.z) * DEGREES_PER_RADIAN
    transform.rotation = Vector3.new(0, yaw, 0)
  end
end
