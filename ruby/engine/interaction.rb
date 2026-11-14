# What players can use, and who uses it. An Interactable is what can be used; an Interactor, on
# the player's hero, picks the best one in reach and uses it. The game hears of every use:
#
#   Interaction.on(:interact) { |actor, target| Debug.log("#{actor.name} used #{target.name}") }
#
# A target is in reach of an actor when it is enabled, no further than its range, in front of the
# actor within its angle (when it has one), and in sight: a ray from the actor to it hits nothing
# but the target (or what is under it) first. Of those, the highest priority wins, then the nearest.
#
# In a game over the network, a client's use is asked of the host, which looks again (range, angle
# and sight, where it sees the client's hero) and runs on_interact there: the host decides what
# happens, and tells the players what they need to see with Network.emit. What Interaction.on
# registers lasts until the next level is loaded.
module Interaction
  ACTION = :interact          # the input action that uses what is in focus
  EVENTS = %i[interact focus blur].freeze
  CALLBACKS = { interact: :on_interact, focus: :on_focus, blur: :on_blur }.freeze
  # How much more the host allows a client's hero, which it sees a moment late: meters past a
  # target's range, and degrees past its angle.
  NETWORK_SLACK = 1.0
  NETWORK_ANGLE_SLACK = 30.0
  # Meters short of a target a ray may stop (on the target's own surface) and still see it.
  SIGHT_TOLERANCE = 0.05
  NEAR = 1e-4                 # meters: closer than this, a target is where the actor is
  DEGREES_PER_RADIAN = 180.0 / Math::PI
  @handlers = {}

  class << self
    # |actor, target| for :interact, :focus and :blur, each with both objects.
    def on(event, &block)
      raise ArgumentError, "Interaction.on takes #{EVENTS.inspect}, not #{event.inspect}" unless EVENTS.include?(event)

      (@handlers[event] ||= []) << block
    end

    # The Interactable an actor (an object) would use now, or nil. lenient: as the host judges a
    # client's hero (see NETWORK_SLACK).
    def best_for(actor, candidates = Interactable.all, lenient: false)
      in_reach = candidates.select { |target| reachable?(actor, target, lenient: lenient) }
      in_reach.min { |one, other| before(actor, one, other) }
    end

    # Whether an actor can use a target now.
    def reachable?(actor, target, lenient: false)
      usable?(actor, target) && within?(actor, target, lenient) && in_front?(actor, target, lenient) &&
        in_sight?(actor, target)
    end

    # The actor uses a target (an Interactable, or an object that has one). Offline and on the
    # host, it happens here when the target is in reach; on a client, the host is asked. False when
    # nothing was done or asked.
    def interact(actor, target)
      target = interactable_of(target)
      return false if target.nil?
      return request(actor, target) if client?
      return false unless reachable?(actor, target)

      tell(:interact, actor, target)
    end

    # Meters from an actor to a target.
    def distance(actor, target) = actor.transform.position.distance_to(target.transform.position)

    # Degrees between where the actor faces and where the target is, across the ground.
    def angle_to(actor, target)
      facing = actor.transform.forward.flat
      toward = (target.transform.position - actor.transform.position).flat
      return 0.0 if [facing, toward].any? { |direction| direction.length < NEAR }

      degrees_between(facing, toward)
    end

    # Called by the Interactor as its focus moves.
    def __focused(actor, target) = tell(:focus, actor, target)
    def __blurred(actor, target) = tell(:blur, actor, target)

    # A client asked the host for its hero (actor) to use one of these objects (those that go by
    # the name it sent): the one in reach, looked for here.
    def __asked(actor, objects)
      target = best_for(actor, objects.filter_map { |object| interactable_of(object) }, lenient: true)
      target ? tell(:interact, actor, target) : false
    end

    # Called by the engine when a level is loaded.
    def __scene_reset
      @handlers = {}
      Interactable.__forget_all
    end

    private

    def usable?(actor, target) = target.enabled? && target.game_object.alive? && target.game_object != actor

    def within?(actor, target, lenient)
      distance(actor, target) <= target.range + (lenient ? NETWORK_SLACK : 0.0)
    end

    def in_front?(actor, target, lenient)
      return true if target.angle.nil?

      angle_to(actor, target) <= target.angle + (lenient ? NETWORK_ANGLE_SLACK : 0.0)
    end

    # How two targets an actor reaches compare: the higher priority first, then the nearer.
    def before(actor, one, other)
      (other.priority <=> one.priority).nonzero? || distance(actor, one) <=> distance(actor, other)
    end

    # Degrees between two directions that are not zero.
    def degrees_between(one, other)
      cosine = one.dot(other) / (one.length * other.length)
      Math.acos(cosine.clamp(-1.0, 1.0)) * DEGREES_PER_RADIAN
    end

    # Whether a ray from the actor reaches the target before anything else. What the actor
    # carries (the objects under it) does not hide anything.
    def in_sight?(actor, target)
      length = distance(actor, target)
      return true if length < NEAR

      direction = (target.transform.position - actor.transform.position) / length
      hit = Physics.raycast(actor.transform.position, direction, length, ignore: carried(actor))
      clear_hit?(hit, target, length)
    end

    def clear_hit?(hit, target, length)
      hit.nil? || hit[:distance] >= length - SIGHT_TOLERANCE || part_of?(hit[:object], target.game_object)
    end

    # Whether an object is the target, or is under it (a character's collider on a child).
    def part_of?(object, whole)
      object = object.parent until object.nil? || object == whole
      !object.nil?
    end

    # An object and everything under it.
    def carried(object) = [object] + object.children.flat_map { |child| carried(child) }

    def interactable_of(target)
      target.is_a?(Interactable) ? target : target&.get_component(Interactable)
    end

    def client? = Network.online? && !Network.host?

    # The host is asked, by the names the two machines know the actor and the target by.
    def request(actor, target)
      keys = [actor.get_component(NetworkSync)&.key, Network.__key_of(target.game_object)]
      return false unless Network::Said.interaction?(keys)

      Network.__deliver(Network::INTERACT, keys, :host)
    end

    # The target's behaviours hear of it (as the engine calls them: one that fails is disabled),
    # then the game's handlers. True.
    def tell(event, actor, target)
      target.game_object.__notify(CALLBACKS[event], actor)
      (@handlers[event] || []).dup.each { |block| block.call(actor, target.game_object) }
      true
    end
  end
end
