# Locked wheels at speed: puffs of smoke left behind a car's rear axle while it brakes hard.
class TireSmoke
  MIN_SPEED = 8.0     # m/s above which hard braking smokes the tires
  SECONDS = 0.04      # between puffs
  REAR_AXLE = 1.3     # meters behind the car's middle
  HEIGHT = 0.45       # meters above the road
  # One puff (size in meters, life in seconds, speed in m/s, spin in degrees per second). In the
  # dusk's light white smoke would shine, so it is gray; its gravity is a share of the world's,
  # upside down: it drifts up.
  PUFF = {
    count: 1, image: :smoke, color: [120, 120, 124].freeze, alpha: 70, speed: 0.8, life: 1.4, size: 1.3,
    gravity: -0.04, spin: 50.0
  }.freeze

  def initialize
    @wait = 0.0
  end

  # Called every frame the wheels are locked, with where the car is, where its nose points and its
  # speed (m/s).
  def puff(position, nose, speed, dt)
    @wait -= dt
    return unless @wait <= 0.0 && speed.abs > MIN_SPEED

    @wait = SECONDS
    Particles.burst(position - nose * REAR_AXLE + Vector3.up * HEIGHT, **PUFF)
  end
end
