# What an NPC is doing, and what makes it do something else: a state machine. A Brain is a
# behaviour; a game's NPC is a Brain with its states and the transitions between them:
#
#   class Guard < Brain
#     initial :patrol
#     state :patrol                    # runs enter_patrol, update_patrol(dt), exit_patrol (when defined)
#     state :chase, update: :run_at_target
#     state :search
#     transition from: :patrol, to: :chase, on: :see        # an event (see fire)
#     transition from: :chase, to: :search, on: :lose_sight
#     transition from: :search, to: :patrol, after: 8.0     # a timer: seconds in the state
#     transition from: :any, to: :search, on: :hear, unless: :chasing?
#     transition from: :patrol, to: :search, if: -> { time_in_state > 30 && tired? }  # a guard
#     transition from: :search, to: :scheduled, after: 60.0  # back to what its Schedule says
#
#     def chasing? = state == :chase
#   end
#
# Each frame the transitions without `on:` are checked in the order they were declared, and the
# first that may be taken is (one a frame); then the state's update runs. One with `on:` is taken
# when that event is fired. A transition may be taken when it starts from the state the brain is in
# (or from :any, which never leads to the state it is in), its `after:` seconds have gone by in the
# state, and its guards agree: `if:` true, `unless:` false. A guard is a method's name (called
# with nothing) or a block run on the brain, which gets the event's payload when it takes an
# argument: `if: ->(seen) { seen.tag?("Player") }`. A hook is a method's name or a block too.
#
# The object's Perception fires :see, :lose_sight (with the object) and :hear (with the noise);
# its Interactable fires :interact (with the actor); its Schedule fires :schedule (with the
# activity), and a transition to: :scheduled goes to the state named after the activity it is time
# for (when the brain has such a state; one from :any or to :scheduled never re-enters a state).
# The game fires its own: brain.fire(:hit, 3).
# Time in a state is game time: it stands still while the game is paused. Over the network only
# the host thinks: on a client a Brain does nothing (the host moves what the players see).
# A game's Brain that has its own update calls super.
class Brain < Behaviour
  ANY = :any
  SCHEDULED = :scheduled   # to: the state named after the activity its Schedule says it is time for
  HOOKS = %i[enter update exit].freeze
  TRANSITION_OPTIONS = %i[on after if unless].freeze

  # A way out of one state (or of any) into another.
  Transition = Struct.new(:from, :to, :event, :after, :if_guard, :unless_guard)

  class << self
    def inherited(subclass)
      super
      subclass.__copy_plan(states.dup, transitions.dup, @initial)
    end

    # The states, by name: { name => { enter:, update:, exit: } }.
    def states = @states ||= {}

    def transitions = @transitions ||= []

    # The state a brain starts in: the first declared unless said.
    def initial(name = nil)
      @initial = name.to_sym if name
      @initial || states.keys.first
    end

    # A state. Its hooks (enter:, update:, exit:) default to the methods enter_<name>,
    # update_<name>(dt) and exit_<name>, when the brain has them.
    def state(name, **hooks)
      unknown = hooks.keys - HOOKS
      raise ArgumentError, "Brain state #{name} takes #{HOOKS.inspect}, not #{unknown.inspect}" unless unknown.empty?

      states[name.to_sym] = hooks
    end

    # A transition: from: a state, a list of them or :any; to: a state; on: an event; after:
    # seconds in the state; if: and unless: guards.
    def transition(from:, to:, **options)
      check_options(options)
      Array(from).each { |source| transitions << transition_of(source, to, options) }
    end

    def __copy_plan(states, transitions, initial)
      @states = states
      @transitions = transitions
      @initial = initial
    end

    private

    def transition_of(from, to, options)
      after = options[:after] && Float(options[:after])
      raise ArgumentError, "Brain transition after: must be from 0 up, not #{after}" if after&.negative?

      Transition.new(from.to_sym, to.to_sym, options[:on]&.to_sym, after, options[:if], options[:unless])
    end

    def check_options(options)
      unknown = options.keys - TRANSITION_OPTIONS
      raise ArgumentError, "Brain transition takes #{TRANSITION_OPTIONS.inspect}, not #{unknown.inspect}" unless
        unknown.empty?
    end
  end

  # The state it is in (nil before its first frame), the one before, and seconds in this one.
  attr_reader :state, :previous_state, :time_in_state

  def update(dt)
    return unless thinks_here?
    return if dt.zero?  # paused: nothing happens, and no time goes by

    awaken if state.nil?
    @time_in_state += dt
    move_to(choice(transitions_on(nil), nil))
    run_hook(:update, dt)
  end

  # Something happened: the first transition waiting for it that may be taken is. payload goes to
  # its guards (what was seen, heard, used). True when the brain changed state.
  def fire(event, payload = nil)
    return false unless thinks_here?

    awaken if state.nil?
    move_to(choice(transitions_on(event.to_sym), payload))
  end

  # Goes to a state now, whatever the transitions say (out of it and in again, if it is there).
  def go(name)
    name = name.to_sym
    raise ArgumentError, "#{self.class} has no state #{name.inspect}" unless self.class.states.key?(name)

    leave
    enter(name)
    true
  end

  def in?(*names) = names.map(&:to_sym).include?(state)

  # What its object's senses and its Interactable tell it.
  def on_see(object) = fire(:see, object)
  def on_lose_sight(object) = fire(:lose_sight, object)
  def on_hear(noise) = fire(:hear, noise)
  def on_interact(actor) = fire(:interact, actor)
  # ... and its Schedule: an activity's hour came.
  def on_schedule(activity) = fire(:schedule, activity)

  private

  # Only one machine decides what an NPC does: the host, or the only one.
  def thinks_here? = !Network.online? || Network.host?

  def awaken
    first = self.class.initial
    raise ArgumentError, "#{self.class} has no states" if first.nil?

    check_plan
    enter(first)
  end

  # A transition that names a state nobody declared is a mistake, found as the brain starts.
  def check_plan
    names = self.class.states.keys
    wrong = self.class.transitions.flat_map { |each| [each.from, each.to] }.uniq - names - [ANY, SCHEDULED]
    raise ArgumentError, "#{self.class} has transitions with unknown states #{wrong.inspect}" unless wrong.empty?
  end

  # The state the first transition that may be taken leads to, or nil.
  def choice(transitions, payload)
    transitions.each do |transition|
      target = target_of(transition, payload)
      return target if target && allowed?(transition, target, payload)
    end
    nil
  end

  # Where a transition leads: its state, or for :scheduled the state of the activity it is time
  # for (the :schedule event's, else the object's Schedule's), when the brain has one of that name.
  def target_of(transition, payload)
    return transition.to unless transition.to == SCHEDULED

    activity = transition.event == :schedule ? payload : get_component(Schedule)&.current
    state_named(activity)
  end

  # The state an activity names, or nil when the brain has none of that name.
  def state_named(activity)
    name = activity&.to_sym
    self.class.states.key?(name) ? name : nil
  end

  def transitions_on(event) = self.class.transitions.select { |transition| transition.event == event }

  def allowed?(transition, target, payload)
    starts_here?(transition, target) && waited?(transition) && guards_agree?(transition, payload)
  end

  # Whether it starts from this state. One from :any, or to :scheduled, never leads to the state
  # the brain is in.
  def starts_here?(transition, target)
    return false unless [state, ANY].include?(transition.from)

    target != state || fixed_self_transition?(transition)
  end

  # A transition from a state back into it, which it names: out of it and in again.
  def fixed_self_transition?(transition) = transition.from == state && transition.to != SCHEDULED

  def waited?(transition) = transition.after.nil? || time_in_state >= transition.after

  def guards_agree?(transition, payload)
    yes = transition.if_guard.nil? || guard(transition.if_guard, payload)
    no = transition.unless_guard && guard(transition.unless_guard, payload)
    yes && !no
  end

  def move_to(target)
    return false if target.nil?

    leave
    enter(target)
    true
  end

  def leave
    run_hook(:exit) if state
  end

  def enter(name)
    @previous_state = state
    @state = name
    @time_in_state = 0.0
    run_hook(:enter)
  end

  # Runs the state's hook: the one it was declared with, else its <hook>_<state> method if any.
  def run_hook(hook, *arguments)
    given = self.class.states.fetch(state, {})[hook]
    named = :"#{hook}_#{state}"
    return call(given, arguments) if given
    return nil unless respond_to?(named, true)

    __send__(named, *arguments)
  end

  # Calls a method by its name with the arguments, or a block on the brain with as many of them
  # as it takes.
  def call(callable, arguments)
    return __send__(callable, *arguments) if callable.is_a?(Symbol)

    instance_exec(*arguments.first(arity_of(callable)), &callable)
  end

  # A guard: a method's name, called with nothing, or a block, which may take the payload.
  def guard(callable, payload)
    callable.is_a?(Symbol) ? __send__(callable) : call(callable, [payload])
  end

  def arity_of(callable) = callable.arity.negative? ? callable.arity.abs - 1 : callable.arity
end
