# The hero: WASD / arrows or a gamepad's left stick to walk, Space or the gamepad's A to jump.
# Needs body = "character" on the object.
class Player < Behaviour
  SPEED = 6.0
  JUMP_SPEED = 8.0
  FALL_LIMIT = -10.0
  DEGREES_PER_RADIAN = 180.0 / Math::PI
  HUD_X = 20
  HUD_Y = 20
  HUD_SIZE = 28

  attr_reader :coins

  def start
    @spawn = transform.position
    @coins = 0
    @total = find_all(:coin).size
  end

  def collect_coin
    @coins += 1
    Audio.play(:coin)
    GameMenu.instance.finish("You got every coin!") if @coins == @total
  end

  # Whether this machine plays the hero: not another player's (see Online), and none on a
  # dedicated server, which plays nobody.
  def played?
    sync = get_component(NetworkSync)
    (sync.nil? || sync.mine?) && !Network.server?
  end

  def update(dt)
    return unless played?
    return draw_hud if dt.zero?  # a menu is open: its buttons are not the hero's

    move(Vector3.new(Input.axis(:horizontal), 0, -Input.axis(:vertical)).clamped)
    transform.position = @spawn if transform.position.y < FALL_LIMIT
    draw_hud
  end

  def draw_hud = UI.text("Coins: #{@coins}/#{@total}", x: HUD_X, y: HUD_Y, size: HUD_SIZE, color: :gold)

  private

  # Walks, jumps and turns the way the player asks.
  def move(direction)
    character.move(direction * prop(:speed, SPEED))
    jump if jump_asked?
    face(direction) unless direction == Vector3.zero
  end

  def jump_asked? = Input.action_pressed?(:jump) && character.grounded?

  def jump
    character.jump(prop(:jump_speed, JUMP_SPEED))
    Audio.play(:jump)
  end

  # Turns the hero so its forward (-Z) points along the movement direction.
  def face(direction)
    yaw = Math.atan2(-direction.x, -direction.z) * DEGREES_PER_RADIAN
    transform.rotation = Vector3.new(0, yaw, 0)
  end
end
