# What an NPC sees and hears. Its sight is a cone in front of its eyes, out to a range, and what is
# in the cone must be in the line of sight (see LineOfSight): a wall hides the player. Its hearing
# is a radius: a noise made with Perception.noise is heard by every Perception close enough.
#
#   guard.add_component(Perception, sight: 15.0, fov: 90.0, hearing: 12.0)
#   Perception.noise(at: transform.position, loudness: 2.0)   # a gunshot: heard twice as far
#
#   class Guard < Behaviour
#     def on_see(object); end          # it came into sight ...
#     def on_lose_sight(object); end   # ... and went out of it
#     def on_hear(noise); end          # noise.position, noise.loudness, noise.source, noise.distance
#   end
#
# A Brain on the object hears of these as the :see, :lose_sight and :hear events.
#
# Settings (each also a Blender property of the object): sight (meters), fov (degrees, the whole
# width of the cone), eyes (meters above the object's origin it looks from), aim (meters above a
# watched object's origin it looks at), hearing (meters a noise of loudness 1 is heard from),
# watch (the tag of what it looks for; the players' heroes, the objects with an Interactor, unless
# said). Over the network only the host perceives, as only its Brains think; on a client no
# Perception sees, and a noise made there is heard by nobody.
class Perception < Behaviour
  SIGHT = 15.0          # meters
  FOV = 120.0           # degrees
  EYES = 1.6            # meters above the origin
  AIM = 1.0             # meters above a watched object's origin
  HEARING = 10.0        # meters, for a noise of loudness 1
  LOUDNESS = 1.0
  HALF = 0.5
  DEGREES_PER_RADIAN = 180.0 / Math::PI
  LARGEST = 1_000_000.0 # meters or degrees: past this, a setting is a mistake
  FULL_TURN = 360.0

  # A noise someone heard: where it was made, how loud, who made it (or nil), and meters away.
  class Noise
    attr_reader :position, :loudness, :source, :distance

    def initialize(position, loudness, source, distance)
      @position = position
      @loudness = loudness
      @source = source
      @distance = distance
    end
  end

  class << self
    # Every Perception of the scene whose object is still there.
    def all = registered.select { |perception| perception.game_object.alive? }

    def registered = @registered ||= []

    # A noise at a point. loudness scales how far it is heard (2: twice each listener's hearing).
    # source (an object) hears nothing of its own noise. The objects that heard it.
    def noise(at:, loudness: LOUDNESS, source: nil)
      loudness = number(loudness, :loudness)
      return [] unless perceives_here?

      all.filter_map { |listener| listener.__hear(at, loudness, source) }
    end

    # The scene is gone, and its Perceptions with it.
    def __scene_reset = @registered = []

    # A number a setting may be: finite, from 0 up, and no more than `most`.
    def number(value, name, most = LARGEST)
      number = Float(value)
      raise ArgumentError, "Perception #{name} must be from 0 to #{most}, not #{value}" unless
        number.between?(0.0, most)

      number
    end

    # Only the host perceives (or the only machine), as only its Brains think.
    def perceives_here? = Brain.thinks_here?
  end

  def start
    [sight, fov, eyes, aim, hearing]  # a setting that is no number stops here
    @in_sight = []
    @last_seen = {}
    Perception.registered << self
  end

  def on_destroy = Perception.registered.delete(self)

  def update(dt)
    return unless Perception.perceives_here?
    return if dt.zero?  # paused: it notices nothing new until the game goes on

    @eye_point = eye_point
    @ignored = game_object.__with_children
    look(watched.select { |object| sees?(object) })
  ensure
    @eye_point = @ignored = nil
  end

  # What it sees now (as of its last update).
  def in_sight = @in_sight.dup

  def sees_any? = !@in_sight.empty?

  # Where it last saw an object (a Vector3), or nil when it never has.
  def last_seen(object) = @last_seen[object]

  # Whether an object is in its sight now: near enough, in the cone, and not hidden.
  def sees?(object)
    object.alive? && object != game_object && within_sight?(object) && in_cone?(object) && visible?(object)
  end

  # Degrees between where it looks and where an object is, across the ground.
  def angle_to(object)
    toward = (target_point(object) - eyes_now).flat
    facing = transform.forward.flat
    return 0.0 if [toward, facing].any? { |direction| direction.length < LineOfSight::NEAR }

    cosine = facing.dot(toward) / (facing.length * toward.length)
    Math.acos(cosine.clamp(-1.0, 1.0)) * DEGREES_PER_RADIAN
  end

  # Where it looks from.
  def eye_point = transform.position + Vector3.up * eyes

  def sight = Perception.number(prop(:sight, SIGHT), :sight)
  def fov = Perception.number(prop(:fov, FOV), :fov, FULL_TURN)
  def eyes = Perception.number(prop(:eyes, EYES), :eyes)
  def aim = Perception.number(prop(:aim, AIM), :aim)
  def hearing = Perception.number(prop(:hearing, HEARING), :hearing)

  # What it looks for.
  def watched
    tag = prop(:watch)
    tag ? Scene.find_all(tag.to_s) : Interactor.all.map(&:game_object)
  end

  # A noise was made: it hears it when near enough (a Noise), else nil. The object it is on when
  # it heard.
  def __hear(position, loudness, source)
    return nil if source == game_object

    distance = transform.position.distance_to(position)
    return nil if distance > hearing * loudness

    game_object.__notify(:on_hear, Noise.new(position, loudness, source, distance))
    game_object
  end

  private

  # What it sees now: those that went out of sight hear of it, then those that came in.
  def look(seen)
    (@in_sight - seen).each { |object| lost(object) }
    (seen - @in_sight).each { |object| game_object.__notify(:on_see, object) }
    @in_sight = seen
    seen.each { |object| @last_seen[object] = object.transform.position }
    @last_seen.select! { |object, _where| object.alive? }  # a destroyed object is forgotten
  end

  # Out of sight, or destroyed.
  def lost(object) = game_object.__notify(:on_lose_sight, object)

  def within_sight?(object) = eyes_now.distance_to(target_point(object)) <= sight

  def in_cone?(object) = angle_to(object) <= fov * HALF

  def visible?(object)
    LineOfSight.clear?(eyes_now, target_point(object), object, ignore: @ignored || game_object.__with_children)
  end

  def target_point(object) = object.transform.position + Vector3.up * aim

  # Where it looks from: worked out once an update, or now when asked between them.
  def eyes_now = @eye_point || eye_point
end
