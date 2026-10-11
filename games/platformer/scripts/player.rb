# Third-person platformer hero: walks with WASD/arrows or a gamepad's left stick, jumps with Space
# or the gamepad's A.
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

  # Another player's hero: it goes where their machine says (see CoOp).
  def ghost?
    sync = get_component(NetworkSync)
    !sync.nil? && !sync.mine?
  end

  def update(dt)
    return if dt.zero?  # a menu is open: its buttons are not the hero's
    return if ghost?

    move(Vector3.new(Input.axis(:horizontal), 0, -Input.axis(:vertical)).clamped)
    respawn if transform.position.y < FALL_LIMIT
  end

  def respawn
    transform.position = @spawn
    @respawns += 1
  end

  private

  # Walks, jumps and turns the way the player asks.
  def move(direction)
    character.move(direction * prop(:speed, SPEED))
    jump if jump_asked?
    face(direction) unless direction == Vector3.zero
  end

  def jump_asked? = Input.action_pressed?(:jump) && character.grounded?

  def jump = character.jump(prop(:jump_speed, JUMP_SPEED))

  # Turns the hero so its forward (-Z) points along the movement direction.
  def face(direction)
    yaw = Math.atan2(-direction.x, -direction.z) * DEGREES_PER_RADIAN
    transform.rotation = Vector3.new(0, yaw, 0)
  end
end
