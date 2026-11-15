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
  SECONDS_PER_HOUR = 3600.0
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
    def hour = (seconds % SECONDS_PER_DAY) / SECONDS_PER_HOUR

    def minute = ((hour % 1) * MINUTES_PER_HOUR).floor

    # The day it is, from 1.
    def day = FIRST_DAY + (seconds / SECONDS_PER_DAY).floor

    def to_s = format(CLOCK_FORMAT, hour.floor, minute)

    # Sets the time of day (and the day, when given).
    def set(hour:, minute: 0, day: self.day)
      check(hour, HOUR_RANGE, :hour)
      check(minute, MINUTE_RANGE, :minute)
      start_at(((day - FIRST_DAY) * HOURS_PER_DAY + hour + minute.to_f / MINUTES_PER_HOUR) * SECONDS_PER_HOUR)
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

    # Back to a saved state.
    def restore(saved)
      start_at(Float(saved[:seconds] || saved["seconds"]))
      @speed = Float(saved[:speed] || saved["speed"] || SPEED)
    end

    # Seconds of the world's time since the first day began.
    def seconds = (@since || START_HOUR * SECONDS_PER_HOUR) + (Time.elapsed - (@set_at || 0.0)) * speed

    private

    def start_at(world_seconds)
      @since = world_seconds
      @set_at = Time.elapsed
    end

    def check(value, range, name)
      raise ArgumentError, "GameClock #{name} must be in #{range}, not #{value.inspect}" unless
        value.is_a?(Numeric) && range.cover?(value)
    end
  end
end
