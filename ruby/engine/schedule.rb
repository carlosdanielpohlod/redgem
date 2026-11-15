# An NPC's day: what it does from what time, by the GameClock. Each entry starts at its time and
# lasts until the next one; the last lasts through midnight until the first.
#
#   baker.add_component(Schedule, plan: { "06:00" => :bake, "12:30" => :sell, "20:00" => :sleep })
#   baker.add_component(Schedule, plan: "06:00 bake, 12:30 sell, 20:00 sleep")  # also a Blender property
#
#   class Baker < Behaviour
#     def on_schedule(activity); end   # the hour of another activity came
#   end
#
# A Brain on the object hears of it as the :schedule event (with the activity), and a transition
# to: :scheduled goes to the state named after the activity it is time for.
class Schedule < Behaviour
  ENTRY_SEPARATOR = ",".freeze
  TIME_SEPARATOR = ":".freeze
  TIME_PARTS = 2   # hours and minutes
  DECIMAL = 10
  MINUTES_PER_HOUR = GameClock::MINUTES_PER_HOUR

  # The activity it is time for now, or nil when the plan is empty.
  def current = Schedule.activity_at(plan, GameClock.minute_of_day)

  # [[minute of the day, activity], ...], in the order of the day.
  def plan = @plan ||= Schedule.parse(prop(:plan, {}))

  def plan=(given)
    @plan = Schedule.parse(given)
    @told = nil
  end

  def start = plan  # a plan that is wrong stops here

  def update(_dt)
    activity = current
    return if activity == @told

    @told = activity
    game_object.__notify(:on_schedule, activity) if activity
  end

  class << self
    # A plan as given (a hash of "HH:MM" => activity, or "HH:MM activity, ..."), in the order of the day.
    def parse(given)
      plan = entries_of(given).map { |time, activity| [minute_of(time), activity.to_sym] }.sort_by(&:first)
      starts = plan.map(&:first)
      raise ArgumentError, "Schedule plan has two entries at one time: #{given.inspect}" unless starts.uniq == starts

      plan
    end

    # What a plan says for a minute of the day: the last entry started, or the day's last one.
    def activity_at(plan, minute_of_day)
      started = plan.select { |start, _activity| start <= minute_of_day }
      (started.last || plan.last)&.last
    end

    # "06:30" as minutes of the day (390).
    def minute_of(time)
      parts = time.to_s.split(TIME_SEPARATOR)
      raise ArgumentError, "Schedule time #{time.inspect} is not HH:MM" unless parts.size == TIME_PARTS

      hours, minutes = parts.map { |part| Integer(part, DECIMAL) }
      check(hours, GameClock::HOUR_RANGE, time)
      check(minutes, GameClock::MINUTE_RANGE, time)
      hours * MINUTES_PER_HOUR + minutes
    end

    private

    # [time, activity] pairs, from a hash or from text.
    def entries_of(given)
      given.is_a?(String) ? given.split(ENTRY_SEPARATOR).map { |entry| split_entry(entry) } : given.to_a
    end

    def check(value, range, time)
      raise ArgumentError, "Schedule time #{time.inspect} is not in the day" unless range.cover?(value)
    end

    def split_entry(entry)
      time, activity, extra = entry.split
      raise ArgumentError, "Schedule entry #{entry.inspect} is not \"HH:MM activity\"" unless activity && extra.nil?

      [time, activity]
    end
  end
end
