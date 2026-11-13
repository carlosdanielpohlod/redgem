# The hero, seen from behind: WASD / arrows or a gamepad's left stick walk the way the camera looks
# (W goes away from it), Space or the gamepad's A jumps. Needs body = "character" on the object
# (the engine's character controller: it walks, climbs steps and falls).
class Player < Behaviour
  SPEED = 6.0
  JUMP_SPEED = 8.0
  FALL_LIMIT = -10.0         # below this height the hero starts again where it began
  DEGREES_PER_RADIAN = 180.0 / Math::PI
  LEVEL_ENOUGH = 0.01        # a camera looking almost straight down has no "forward" to walk to
  HINT_X = 20
  HINT_Y = 20
  HINT_SIZE = 22

  # Where W, or the stick pushed up, walks to: away from a camera that looks along `forward`
  # (flat, length 1). `horizontal` and `vertical` are Input.axis' (-1 to 1); the result is no
  # longer than 1.
  def self.direction(forward, horizontal, vertical)
    right = forward.cross(Vector3.up)
    (forward * vertical + right * horizontal).clamped
  end

  def start
    @spawn = transform.position
  end

  def update(dt)
    return if dt.zero? # the game is paused: the menu has the keys

    move(Player.direction(camera_forward, Input.axis(:horizontal), Input.axis(:vertical)))
    transform.position = @spawn if transform.position.y < FALL_LIMIT
    UI.text(I18n.t("hud.controls"), x: HINT_X, y: HINT_Y, size: HINT_SIZE)
  end

  private

  # Walks, jumps and turns the way the player asks.
  def move(direction)
    character.move(direction * prop(:speed, SPEED))
    character.jump(prop(:jump_speed, JUMP_SPEED)) if jump_asked?
    face(direction) unless direction == Vector3.zero
  end

  def jump_asked? = Input.action_pressed?(:jump) && character.grounded?

  # Where the camera looks, along the ground.
  def camera_forward
    look = Camera.main.transform.forward.flat
    level?(look) ? look.normalized : Vector3.forward
  end

  def level?(look) = look.length > LEVEL_ENOUGH

  # Turns the hero so its forward (-Z) points along the way it walks.
  def face(direction)
    yaw = Math.atan2(-direction.x, -direction.z) * DEGREES_PER_RADIAN
    transform.rotation = Vector3.new(0, yaw, 0)
  end
end
