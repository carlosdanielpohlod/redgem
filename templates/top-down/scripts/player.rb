# The hero, seen from above. WASD / arrows or a gamepad's left stick walk (W is up the screen);
# a left click on the ground walks there by itself, until the keys or the stick take over again,
# or something in the way stops it. Needs body = "character" on the object (the engine's
# character controller).
class Player < Behaviour
  SPEED = 6.0
  ARRIVED = 0.3              # meters from the spot clicked that count as there
  STUCK_SECONDS = 0.5        # getting no nearer the spot for this long, the hero gives up
  PROGRESS = 0.05            # meters nearer the spot that count as getting nearer
  FLOOR_UP = 0.7             # how much a surface must face up to be walked to (1: flat ground)
  CLICK = :left              # the mouse button that picks where to walk
  MARK_SPARKS = 12           # the burst that shows where a click goes
  FALL_LIMIT = -10.0         # below this height the hero starts again where it began
  DEGREES_PER_RADIAN = 180.0 / Math::PI
  HINT_X = 20
  HINT_Y = 20
  HINT_SIZE = 22

  # Where a click sent the hero, or nil while the keys steer it.
  attr_reader :destination

  # The way along the ground from one point to another, for a hero that goes `reach` meters this
  # frame: length 1 while the spot is further than that, shorter for the last step (so that it
  # stops on the spot instead of stepping past it), and none (Vector3.zero) once the spot is
  # `arrived` meters away or nearer.
  def self.toward(from, to, reach, arrived = ARRIVED)
    way = (to - from).flat
    return Vector3.zero if way.length <= arrived

    way.length > reach ? way.normalized : way / reach
  end

  def start
    @spawn = transform.position
  end

  def update(dt)
    return if dt.zero? # the game is paused: the menu has the keys and the mouse

    pick_destination if Input.mouse_pressed?(CLICK)
    move(steering(dt))
    transform.position = @spawn if transform.position.y < FALL_LIMIT
    UI.text(I18n.t("hud.controls"), x: HINT_X, y: HINT_Y, size: HINT_SIZE)
  end

  # Walks to a point of the ground by itself (what a click does).
  def walk_to(point)
    @destination = point
    @closest = Float::INFINITY
    @stuck_for = 0.0
    Particles.burst(point, count: MARK_SPARKS, color: :white)
  end

  private

  # Where the hero goes this frame: where the keys say; else, after a click, to the spot.
  def steering(dt)
    keys = Vector3.new(Input.axis(:horizontal), 0, -Input.axis(:vertical)).clamped
    @destination = nil unless keys == Vector3.zero
    @destination ? to_destination(dt) : keys
  end

  def to_destination(dt)
    way = Player.toward(transform.position, @destination, prop(:speed, SPEED) * dt)
    @destination = nil if done_walking?(way, dt)
    way
  end

  # Whether the walk to the spot is over: the hero is there, or cannot get there.
  def done_walking?(way, dt) = way == Vector3.zero || stuck?(dt)

  # Whether the hero got no nearer the spot for STUCK_SECONDS (something is in the way).
  def stuck?(dt)
    left = transform.position.flat.distance_to(@destination.flat)
    if left < @closest - PROGRESS
      @closest = left
      @stuck_for = 0.0
    else
      @stuck_for += dt
    end
    @stuck_for > STUCK_SECONDS
  end

  # The spot of the ground under the mouse (a wall's side, or the mouse over the sky, is none).
  def pick_destination
    origin, direction = Camera.screen_ray(Input.mouse_x, Input.mouse_y)
    return if origin.nil? # no screen to click on

    hit = Physics.raycast(origin, direction, ignore: game_object)
    walk_to(hit[:point]) if floor?(hit)
  end

  def floor?(hit) = !hit.nil? && hit[:normal].y >= FLOOR_UP

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
