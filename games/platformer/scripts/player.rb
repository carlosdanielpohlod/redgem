# Third-person platformer hero: walks with WASD/arrows or a gamepad's left stick, jumps with Space
# or the gamepad's A.
# Blender props: speed, jump_speed.
class Player < Behaviour
  SPEED = 6.0
  JUMP_SPEED = 9.0
  FALL_LIMIT = -10.0
  FALL_SHAKE = 0.6        # how hard the camera shakes when the hero falls off and starts again (0 to 1)
  DEGREES_PER_RADIAN = 180.0 / Math::PI

  attr_reader :respawns

  def start
    @spawn = transform.position
    @respawns = 0
  end

  # Whether an object is the hero this machine plays: not something else, nor another player's hero.
  def self.own?(object)
    player = object.get_component(Player)
    !player.nil? && player.played?
  end

  # Another player's hero: it goes where their machine says (see CoOp).
  def ghost?
    sync = get_component(NetworkSync)
    !sync.nil? && !sync.mine?
  end

  # Whether this machine plays the hero: its own, and not on a dedicated server, which plays
  # nobody (its copy of the hero stands by, as what the others' heroes are copied from).
  def played? = !ghost? && !Network.server?

  def update(dt)
    return if dt.zero?  # a menu is open: its buttons are not the hero's
    return unless played?

    move(Vector3.new(Input.axis(:horizontal), 0, -Input.axis(:vertical)).clamped)
    respawn if transform.position.y < FALL_LIMIT
  end

  def respawn
    transform.position = @spawn
    @respawns += 1
    Camera.shake(FALL_SHAKE)
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
