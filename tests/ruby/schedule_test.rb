# The game clock (time of day, days, speed, saving) and the schedules that follow it, with a Brain
# that goes where its schedule says. Game time is the test's: Time.elapsed is what it sets.
class Time
  class << self
    attr_writer :fake_elapsed

    def elapsed = @fake_elapsed || 0.0
  end
end

# What a scene object answers that a schedule asks.
class PlannedObject
  attr_reader :heard, :behaviours

  def initialize
    @heard = []
    @behaviours = []
  end

  def props = {}
  def get_component(klass) = @behaviours.find { |behaviour| behaviour.is_a?(klass) }
  def __add_behaviour(behaviour) = @behaviours << behaviour

  def __notify(callback, argument)
    @heard << [callback, argument]
    @behaviours.each { |behaviour| behaviour.__send__(callback, argument) }
  end
end

# Works, eats and sleeps as its schedule says, and runs off when scared.
class VillagerBrain < Brain
  RECOVERY = 5.0 # seconds scared
  state :work
  state :eat
  state :sleep
  state :flee
  transition from: %i[work eat sleep], to: :scheduled, on: :schedule
  transition from: :any, to: :flee, on: :scare
  transition from: :flee, to: :scheduled, after: RECOVERY
end

class ScheduleTest < RedGemTest::Case
  PLAN = { "06:00" => :work, "12:30" => :eat, "20:00" => :sleep }.freeze
  FRAME = 1.0 / 60
  GAME_MINUTE = 1.0 # seconds of game time, at the clock's own speed
  HOUR = 60 * GAME_MINUTE
  STOPPED = 0.0

  def setup
    Time.fake_elapsed = 0.0
    GameClock.speed = GameClock::SPEED
    GameClock.set(hour: 6, day: 1)
  end

  def test_the_clock_runs_a_game_minute_each_second
    assert_equal "06:00", GameClock.to_s
    pass(HOUR + 30 * GAME_MINUTE)
    assert_equal "07:30", GameClock.to_s
    assert_in_delta 7.5, GameClock.hour
    assert_equal 30, GameClock.minute
  end

  def test_days_go_by_past_midnight
    GameClock.set(hour: 23, minute: 30)
    pass(HOUR)
    assert_equal "00:30", GameClock.to_s
    assert_equal 2, GameClock.day
  end

  def test_a_new_speed_keeps_the_time_it_is
    pass(HOUR)
    GameClock.speed = STOPPED
    pass(10 * HOUR)
    assert_equal "07:00", GameClock.to_s
  end

  def test_the_clock_saves_and_restores
    GameClock.set(hour: 18, minute: 15, day: 3)
    saved = GameClock.state
    GameClock.set(hour: 1)
    GameClock.restore(saved)
    assert_equal "18:15", GameClock.to_s
    assert_equal 3, GameClock.day
  end

  def test_wrong_times_raise
    assert_raises(ArgumentError) { GameClock.set(hour: 24) }
    assert_raises(ArgumentError) { GameClock.set(hour: 6, minute: 60) }
    assert_raises(ArgumentError) { GameClock.speed = -1 }
  end

  def test_a_plan_says_what_it_is_time_for_through_midnight
    plan = Schedule.parse(PLAN)
    assert_equal :work, Schedule.activity_at(plan, 6.0)
    assert_equal :eat, Schedule.activity_at(plan, 13.0)
    assert_equal :sleep, Schedule.activity_at(plan, 23.0)
    assert_equal :sleep, Schedule.activity_at(plan, 3.0), "the night goes on from the day before"
  end

  def test_a_plan_may_be_text_as_a_blender_property_is
    assert_equal Schedule.parse(PLAN), Schedule.parse("20:00 sleep, 06:00 work, 12:30 eat")
  end

  def test_wrong_plans_raise
    assert_raises(ArgumentError) { Schedule.parse("6 work") }
    assert_raises(ArgumentError) { Schedule.parse("25:00 work") }
    assert_raises(ArgumentError) { Schedule.parse("06:00 work hard") }
  end

  def test_a_schedule_tells_its_object_when_an_activity_starts
    villager = PlannedObject.new
    schedule = with(villager, Schedule.new, plan: PLAN)
    schedule.update(FRAME)
    GameClock.set(hour: 12, minute: 30)
    schedule.update(FRAME)
    schedule.update(FRAME)
    assert_equal [%i[on_schedule work], %i[on_schedule eat]], villager.heard
  end

  def test_a_brain_follows_its_schedule
    villager = PlannedObject.new
    schedule = with(villager, Schedule.new, plan: PLAN)
    brain = with(villager, VillagerBrain.new)
    GameClock.set(hour: 13)
    schedule.update(FRAME)
    assert_equal :eat, brain.state
  end

  def test_a_scared_brain_goes_back_to_what_its_schedule_says_now
    schedule, brain = scared_villager
    GameClock.set(hour: 21)
    schedule.update(FRAME)
    assert_equal :flee, brain.state, "a scared villager does not go to sleep"
    brain.update(VillagerBrain::RECOVERY)
    assert_equal :sleep, brain.state
  end

  private

  def scared_villager
    villager = PlannedObject.new
    schedule = with(villager, Schedule.new, plan: PLAN)
    brain = with(villager, VillagerBrain.new)
    brain.fire(:scare)
    [schedule, brain]
  end

  def pass(seconds)
    Time.fake_elapsed = Time.elapsed + seconds
  end

  def with(object, behaviour, **settings)
    behaviour.__configure(settings)
    behaviour.__attach(object)
    behaviour
  end
end
