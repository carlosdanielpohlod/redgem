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
  DECIMAL = 10
  MINUTES_PER_HOUR = GameClock::MINUTES_PER_HOUR

  # The activity it is time for now, or nil when the plan is empty.
  def current = Schedule.activity_at(plan, GameClock.hour)

  # [[hour, activity], ...], in the order of the day.
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
      pairs = given.is_a?(String) ? given.split(ENTRY_SEPARATOR).map { |entry| split_entry(entry) } : given.to_a
      pairs.map { |time, activity| [hour_of(time), activity.to_sym] }.sort_by(&:first)
    end

    # What a plan says for an hour of the day: the last entry started, or the day's last one.
    def activity_at(plan, hour)
      started = plan.select { |start, _activity| start <= hour }
      (started.last || plan.last)&.last
    end

    # "06:30" (or 6.5) as hours.
    def hour_of(time)
      return check_hour(Float(time), time) if time.is_a?(Numeric)

      hours, minutes = time.to_s.split(TIME_SEPARATOR).map { |part| Integer(part, DECIMAL) }
      raise ArgumentError, "Schedule time #{time.inspect} is not HH:MM" unless minute?(minutes)

      check_hour(hours + minutes.to_f / MINUTES_PER_HOUR, time)
    end

    private

    def minute?(minutes) = !minutes.nil? && minutes.between?(0, MINUTES_PER_HOUR - 1)

    def split_entry(entry)
      time, activity, extra = entry.split
      raise ArgumentError, "Schedule entry #{entry.inspect} is not \"HH:MM activity\"" unless activity && extra.nil?

      [time, activity]
    end

    def check_hour(hour, given)
      raise ArgumentError, "Schedule time #{given.inspect} is not in the day" unless GameClock::HOUR_RANGE.cover?(hour)

      hour
    end
  end
end
