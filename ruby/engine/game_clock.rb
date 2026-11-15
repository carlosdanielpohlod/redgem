# The time of day in the game's world: what NPCs' schedules follow. It runs on game time (it
# stands still while the game is paused), from 06:00 of day 1 unless set, one game minute for each
# second unless said, and it goes on across levels.
#
#   GameClock.hour                       # => 6.5 (half past six)
#   GameClock.to_s                       # => "06:30"
#   GameClock.day                        # => 1
#   GameClock.set(hour: 21, minute: 30)  # night falls
#   GameClock.speed = 120.0              # game seconds for each second: a day in 12 minutes
#
# Saving it is the game's: Save.set(:clock, GameClock.state), then GameClock.restore(Save.get(:clock)).
# Over the network each machine keeps its own clock: the host tells the others the time.
module GameClock
  HOURS_PER_DAY = 24
  MINUTES_PER_HOUR = 60
  SECONDS_PER_MINUTE = 60
  SECONDS_PER_HOUR = SECONDS_PER_MINUTE * MINUTES_PER_HOUR
  MINUTES_PER_DAY = HOURS_PER_DAY * MINUTES_PER_HOUR
  SECONDS_PER_DAY = HOURS_PER_DAY * SECONDS_PER_HOUR
  START_HOUR = 6
  FIRST_DAY = 1
  SPEED = 60.0          # game seconds for each second of game time
  FASTEST = 1_000_000.0 # past this, a speed is a mistake
  HOUR_RANGE = (0...HOURS_PER_DAY)
  MINUTE_RANGE = (0...MINUTES_PER_HOUR)
  CLOCK_FORMAT = "%02d:%02d".freeze

  class << self
    # The hour of the day, with its minutes as a fraction: 0 up to (not including) 24.
    def hour = time_of_day.to_f / SECONDS_PER_HOUR

    # The minute of the hour.
    def minute = minute_of_day % MINUTES_PER_HOUR

    # Whole minutes since midnight: 0 up to 1439.
    def minute_of_day = (time_of_day / SECONDS_PER_MINUTE).floor

    # The day it is, from 1.
    def day = FIRST_DAY + (seconds / SECONDS_PER_DAY).floor

    def to_s = format(CLOCK_FORMAT, minute_of_day / MINUTES_PER_HOUR, minute)

    # Sets the time of day, in whole hours and minutes (and the day, from 1, when given).
    def set(hour:, minute: 0, day: self.day)
      check(hour, HOUR_RANGE, :hour)
      check(minute, MINUTE_RANGE, :minute)
      raise ArgumentError, "GameClock day must be a whole number from 1, not #{day.inspect}" unless whole_day?(day)

      start_at((day - FIRST_DAY) * SECONDS_PER_DAY + hour * SECONDS_PER_HOUR + minute * SECONDS_PER_MINUTE)
    end

    def speed = @speed || SPEED

    # Game seconds for each second of game time (0 stops the clock).
    def speed=(value)
      value = Float(value)
      raise ArgumentError, "GameClock.speed must be from 0 to #{FASTEST}, not #{value}" unless
        value.between?(0.0, FASTEST)

      start_at(seconds)
      @speed = value
    end

    # What to save: { seconds:, speed: }.
    def state = { seconds: seconds, speed: speed }

    # Back to a saved state; nil (nothing saved yet) leaves the clock as it is.
    def restore(saved)
      return if saved.nil?

      self.speed = saved[:speed] || saved["speed"] || SPEED
      start_at(Float(saved[:seconds] || saved["seconds"]))
    end

    # Seconds of the world's time since the first day began.
    def seconds = (@since || START_HOUR * SECONDS_PER_HOUR) + (Time.elapsed - (@set_at || 0.0)) * speed

    private

    def time_of_day = seconds % SECONDS_PER_DAY

    def start_at(world_seconds)
      raise ArgumentError, "GameClock cannot go before its first day, to #{world_seconds}" if world_seconds.negative?

      @since = world_seconds
      @set_at = Time.elapsed
    end

    def check(value, range, name)
      raise ArgumentError, "GameClock #{name} must be a whole number in #{range}, not #{value.inspect}" unless
        value.is_a?(Integer) && range.cover?(value)
    end

    def whole_day?(day) = day.is_a?(Integer) && day >= FIRST_DAY
  end
end
