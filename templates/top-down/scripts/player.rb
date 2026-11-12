# The hero, seen from above. WASD / arrows or a gamepad's left stick walk (W is up the screen);
# a left click on the ground walks there by itself, until the keys or the stick take over again.
# Needs body = "character" on the object (the engine's character controller).
class Player < Behaviour
  SPEED = 6.0
  ARRIVED = 0.3              # meters from the spot clicked that count as there
  CLICK = :left              # the mouse button that picks where to walk
  MARK_SPARKS = 12           # the burst that shows where a click goes
  FALL_LIMIT = -10.0         # below this height the hero starts again where it began
  DEGREES_PER_RADIAN = 180.0 / Math::PI
  HINT_X = 20
  HINT_Y = 20
  HINT_SIZE = 22

  # Where a click sent the hero, or nil while the keys steer it.
  attr_reader :destination

  # The way along the ground from one point to another (length 1), or none (Vector3.zero) once
  # they are no more than `arrived` meters apart.
  def self.toward(from, to, arrived = ARRIVED)
    way = (to - from).flat
    way.length <= arrived ? Vector3.zero : way.normalized
  end

  def start
    @spawn = transform.position
  end

  def update(dt)
    return if dt.zero? # the game is paused: the menu has the keys and the mouse

    pick_destination if Input.mouse_pressed?(CLICK)
    move(steering)
    transform.position = @spawn if transform.position.y < FALL_LIMIT
    UI.text(I18n.t("hud.controls"), x: HINT_X, y: HINT_Y, size: HINT_SIZE)
  end

  # Walks to a point of the ground by itself (what a click does).
  def walk_to(point)
    @destination = point
    Particles.burst(point, count: MARK_SPARKS, color: :white)
  end

  private

  # Where the hero goes this frame: where the keys say; else, after a click, to the spot.
  def steering
    keys = Vector3.new(Input.axis(:horizontal), 0, -Input.axis(:vertical)).clamped
    @destination = nil unless keys == Vector3.zero
    @destination ? to_destination : keys
  end

  def to_destination
    way = Player.toward(transform.position, @destination)
    @destination = nil if way == Vector3.zero
    way
  end

  # The spot of the ground (or of anything on it) under the mouse.
  def pick_destination
    origin, direction = Camera.screen_ray(Input.mouse_x, Input.mouse_y)
    return if origin.nil? # no screen to click on

    hit = Physics.raycast(origin, direction, ignore: game_object)
    walk_to(hit[:point]) if hit
  end

  # Walks, and turns the way it walks.
  def move(direction)
    character.move(direction * prop(:speed, SPEED))
    face(direction) unless direction == Vector3.zero
  end

  # Turns the hero so its forward (-Z) points along the way it walks.
  def face(direction)
    yaw = Math.atan2(-direction.x, -direction.z) * DEGREES_PER_RADIAN
    transform.rotation = Vector3.new(0, yaw, 0)
  end
end
