# The sound of the player's car's engine: its note climbs through each gear and drops when the
# next one comes in, and it is louder with the throttle down.
class EngineSound
  GEARS = [9.0, 16.0, 23.0, 30.5].freeze   # top speed of each gear (m/s)
  SOUND = :engine
  IDLE_PITCH = 0.55
  LOW_REV_PITCH = 0.8       # the pitch at the bottom of a gear ...
  HIGH_REV_PITCH = 1.55     # ... and at its top
  MOVING_SPEED = 0.5        # m/s below which the engine idles
  IDLE_VOLUME = 0.22
  DRIVING_VOLUME = 0.5
  VOLUME_QUICKNESS = 4.0    # how fast the loudness follows the throttle (per second)

  # The gear the engine is in at a speed (1 is the first).
  def self.gear(speed) = GEARS.index { |top| speed.abs < top }.to_i + 1

  def initialize
    @volume = IDLE_VOLUME
    Audio.music(SOUND, loop: true, volume: @volume)
  end

  # Called every frame with the car's speed (m/s) and what the pedals ask (-1 to 1).
  def rev(speed, throttle, dt)
    Audio.music_pitch = pitch_at(speed.abs)
    loudness = throttle.zero? ? IDLE_VOLUME : DRIVING_VOLUME
    @volume += (loudness - @volume) * [VOLUME_QUICKNESS * dt, 1.0].min
    Audio.music_volume = @volume
  end

  private

  def pitch_at(speed)
    speed > MOVING_SPEED ? LOW_REV_PITCH + (HIGH_REV_PITCH - LOW_REV_PITCH) * revs(speed) : IDLE_PITCH
  end

  # How far through its gear the engine is, from 0 to 1.
  def revs(speed)
    gear = EngineSound.gear(speed) - 1   # counted from 0, as GEARS is
    bottom = gear.zero? ? 0.0 : GEARS[gear - 1]
    ((speed - bottom) / (GEARS[gear] - bottom)).clamp(0.0, 1.0)
  end
end
