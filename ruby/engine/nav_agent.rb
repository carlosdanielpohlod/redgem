# A character that walks where it is sent, among others: along Navigation.path, steering clear of
# the other agents and of the bodies that move (Navigation.steer), and finding another way around
# what stands in its way (a crate pushed into a doorway, a crowd that does not move). Its object
# walks by its character controller, or by its transform when it has none.
#   guard = Scene.find("Guard").add_component(NavAgent, speed: 2.5)
#   guard.go_to(Scene.find("Gate").transform.position)   # => false when there is no way there
#   guard.arrived?
# Settings (add_component's, else the object's Blender properties): speed (meters per second),
# radius (meters) and arrive_distance (how near the goal is near enough, in meters).
class NavAgent < Behaviour
  SPEED = 3.0
  RADIUS = 0.4
  ARRIVE_DISTANCE = 0.3
  BRAKING = 2.0 # per second: how soon it slows down as it comes to the goal
  STUCK_SECONDS = 1.0 # how often it checks it is getting somewhere
  STUCK_SHARE = 0.25 # less than this share of the way it would walk meanwhile is stuck
  BLOCKER_REACH = 2.0 # meters past its radius in which what moves is gone around when stuck

  @agents = []

  # The agents of the running scene, and each one's [position, velocity, radius] this frame (as
  # Navigation.steer takes them).
  def self.all = @agents.select! { |agent| agent.game_object.alive? } || @agents

  def self.crowd
    return @crowd if @crowd_frame == Time.frame

    @crowd_frame = Time.frame
    agents = all
    @objects = agents.map(&:game_object)
    @crowd = agents.to_h { |agent| [agent, agent.__crowd_entry] }
  end

  # Their objects, this frame.
  def self.objects
    crowd
    @objects
  end

  def self.__join(agent) = @agents << agent
  def self.__leave(agent) = @agents.delete(agent)

  attr_reader :destination

  def start = NavAgent.__join(self)
  def on_destroy = NavAgent.__leave(self)

  # Sends it to a point; true when there is a way there.
  def go_to(point)
    @destination = point
    @arrived = false
    @waypoints = Navigation.path(transform.position, point) || []
    __reset_watch
    @waypoints.any?
  end

  def stop
    @destination = nil
    @waypoints = []
  end

  def arrived? = @arrived == true
  def walking? = waypoints.any?
  def waypoints = @waypoints ||= []
  def velocity = @velocity || Vector3.zero
  def speed = Float(prop(:speed, SPEED))
  def radius = Float(prop(:radius, RADIUS))
  def arrive_distance = Float(prop(:arrive_distance, ARRIVE_DISTANCE))

  def update(dt)
    __follow_path
    @velocity = __steer
    __walk(dt)
    __watch_progress(dt)
  end

  def __crowd_entry = [transform.position, velocity, radius]

  private

  def __steer
    Navigation.steer(transform.position, velocity, __preferred, speed: speed, radius: radius,
                                                                crowd: __others, ignore: NavAgent.objects)
  end

  def __others = NavAgent.crowd.filter_map { |agent, entry| entry unless agent.equal?(self) }

  def __reached?(point, within) = (point - transform.position).flat.length < within

  # Drops the waypoints it reached; the last one reached, it arrived.
  def __follow_path
    waypoints.shift while waypoints.size > 1 && __reached?(waypoints.first, [arrive_distance, radius].max)
    return unless waypoints.size == 1 && __reached?(waypoints.first, arrive_distance)

    waypoints.clear
    @arrived = true
  end

  # Toward the next waypoint, slowing down to stop on the last.
  def __preferred
    target = waypoints.first
    return Vector3.zero unless target

    away = (target - transform.position).flat
    away.normalized * (waypoints.size == 1 ? [speed, away.length * BRAKING].min : speed)
  end

  def __walk(dt)
    return character.move(@velocity) if character

    transform.position = transform.position + (@velocity * dt)
  end

  def __watch_progress(dt)
    return __reset_watch unless walking?

    @watched += dt
    return if @watched < STUCK_SECONDS

    __find_way_around if __stuck?
    __reset_watch
  end

  def __reset_watch
    @watched = 0.0
    @watched_from = transform.position
  end

  def __stuck? = !__braking? && transform.position.distance_to(@watched_from) < __expected_progress

  def __expected_progress = speed * STUCK_SECONDS * STUCK_SHARE

  # Whether it is slowing down to stop on its goal (and goes slowly on purpose).
  def __braking? = waypoints.size == 1 && __reached?(waypoints.first, speed / BRAKING)

  # Another way to the goal, around what moves near it (other agents too).
  def __find_way_around
    found = Navigation.path(transform.position, @destination, avoid: __blockers)
    @waypoints = found if found
  end

  def __blockers
    near = Physics.overlap_sphere(transform.position, radius + BLOCKER_REACH, ignore: game_object)
    near.map { |hit| hit[:object] }.select { |object| object.rigidbody || object.character }
  end
end
