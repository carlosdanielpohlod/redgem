# The hero: WASD / arrows to walk, Space to jump. Needs body = "character" on the object.
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
    GameMenu.instance&.finish("You got every coin!") if @coins == @total
  end

  def update(_dt)
    direction = Vector3.new(Input.axis(:horizontal), 0, -Input.axis(:vertical)).normalized
    character.move(direction * prop(:speed, SPEED))
    if Input.action_pressed?(:jump) && character.grounded?
      character.jump(prop(:jump_speed, JUMP_SPEED))
      Audio.play(:jump)
    end
    unless direction == Vector3.zero
      transform.rotation = Vector3.new(0, Math.atan2(-direction.x, -direction.z) * DEGREES_PER_RADIAN, 0)
    end
    transform.position = @spawn if transform.position.y < FALL_LIMIT
    UI.text("Coins: #{@coins}/#{@total}", x: HUD_X, y: HUD_Y, size: HUD_SIZE, color: :gold)
  end
end
