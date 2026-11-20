# How a Brain moves between its states: polled transitions with guards, timers, events with a
# payload, :any, hooks by name or by block, and the mistakes it refuses.
class GuardBrain < Brain
  WAIT = 2.0        # seconds the guard searches before it goes back to its patrol
  initial :patrol
  state :patrol
  state :chase, enter: :shout, update: ->(dt) { @chased += dt }
  state :search
  state :stunned, exit: -> { @log << :woke }
  transition from: :patrol, to: :chase, on: :see, if: ->(seen) { seen == :hero }
  transition from: :chase, to: :search, on: :lose_sight
  transition from: :search, to: :patrol, after: WAIT
  transition from: :any, to: :stunned, on: :hit
  transition from: :stunned, to: :patrol, if: :awake?, unless: -> { @dazed }

  attr_accessor :awake, :dazed
  attr_reader :log, :chased

  def initialize
    super
    @log = []
    @chased = 0.0
  end

  def awake? = @awake
  def enter_patrol = @log << :patrolling
  def exit_patrol = @log << :left_patrol
  def update_patrol(_dt) = @log << :walking
  def shout = @log << :shout
end

# A brain that inherits its plan and adds to it.
class CaptainBrain < GuardBrain
  state :command
  transition from: :patrol, to: :command, on: :order
end

class BrainTest < RedGemTest::Case
  FRAME = 1.0 / 60
  PAUSED = 0.0

  def setup
    @brain = GuardBrain.new
  end

  def test_it_starts_in_its_initial_state_on_its_first_frame
    assert_nil @brain.state
    @brain.update(FRAME)
    assert_equal :patrol, @brain.state
    assert_equal %i[patrolling walking], @brain.log
  end

  def test_an_event_passes_its_payload_to_the_guard
    @brain.update(FRAME)
    refute @brain.fire(:see, :rabbit), "a rabbit is no reason to run"
    assert @brain.fire(:see, :hero)
    assert_equal :chase, @brain.state
    assert_equal :patrol, @brain.previous_state
    assert_equal %i[patrolling walking left_patrol shout], @brain.log
  end

  def test_a_declared_update_hook_runs_every_frame
    @brain.go(:chase)
    @brain.update(FRAME)
    @brain.update(FRAME)
    assert_in_delta 2 * FRAME, @brain.chased
  end

  def test_a_timer_waits_its_seconds_in_the_state
    @brain.go(:search)
    @brain.update(GuardBrain::WAIT - FRAME)
    assert_equal :search, @brain.state
    @brain.update(FRAME)
    assert_equal :patrol, @brain.state
    assert_in_delta 0.0, @brain.time_in_state
  end

  def test_no_time_goes_by_while_paused
    @brain.go(:search)
    @brain.update(PAUSED)
    assert_in_delta 0.0, @brain.time_in_state
  end

  def test_an_if_guard_must_agree
    @brain.go(:stunned)
    @brain.update(FRAME)
    assert_equal :stunned, @brain.state
    @brain.awake = true
    @brain.update(FRAME)
    assert_equal :patrol, @brain.state
    assert @brain.log.include?(:woke), "the exit hook ran"
  end

  def test_an_unless_guard_must_not
    @brain.go(:stunned)
    @brain.awake = true
    @brain.dazed = true
    @brain.update(FRAME)
    assert_equal :stunned, @brain.state
  end

  def test_any_reaches_every_state_but_the_one_it_is_in
    @brain.go(:search)
    assert @brain.fire(:hit)
    assert_equal :stunned, @brain.state
    refute @brain.fire(:hit), "already stunned: no new stun"
  end

  def test_an_event_nobody_waits_for_does_nothing
    @brain.update(FRAME)
    refute @brain.fire(:lose_sight)
    assert_equal :patrol, @brain.state
  end

  def test_what_it_sees_is_an_event
    @brain.update(FRAME)
    @brain.on_see(:hero)
    assert_equal :chase, @brain.state
    @brain.on_lose_sight(:hero)
    assert_equal :search, @brain.state
  end

  def test_a_subclass_keeps_its_parents_plan
    captain = CaptainBrain.new
    captain.update(FRAME)
    assert_equal :patrol, captain.state
    assert captain.fire(:order)
    assert captain.in?(:command, :chase)
    refute GuardBrain.states.key?(:command), "the parent is unchanged"
  end

  def test_going_to_an_unknown_state_raises
    assert_raises(ArgumentError) { @brain.go(:dance) }
  end

  def test_a_transition_to_an_unknown_state_raises_as_the_brain_starts
    broken = Class.new(Brain) do
      state :idle
      transition from: :idle, to: :nowhere, after: 1.0
    end
    assert_raises(ArgumentError) { broken.new.update(FRAME) }
  end

  def test_any_is_no_state_to_go_to
    to_any = Class.new(Brain) do
      state :idle
      transition from: :idle, to: :any, on: :hit
    end
    assert_raises(ArgumentError) { to_any.new.update(FRAME) }
  end

  def test_the_initial_state_must_be_declared
    misspelled = Class.new(Brain) do
      initial :idel
      state :idle
    end
    assert_raises(ArgumentError) { misspelled.new.update(FRAME) }
  end

  def test_time_in_state_is_zero_before_the_first_frame
    assert_in_delta 0.0, @brain.time_in_state
  end

  def test_a_brain_without_states_raises
    assert_raises(ArgumentError) { Class.new(Brain).new.update(FRAME) }
  end

  def test_wrong_options_raise_as_they_are_declared
    assert_raises(ArgumentError) { Class.new(Brain) { state :idle, enter: :a, start: :b } }
    assert_raises(ArgumentError) { Class.new(Brain) { transition from: :a, to: :b, when: :c } }
    assert_raises(ArgumentError) { Class.new(Brain) { transition from: :a, to: :b, after: -1 } }
  end
end
