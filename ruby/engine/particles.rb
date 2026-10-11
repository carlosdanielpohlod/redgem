# Small things that fly and fade: sparks, dust, smoke, confetti. They are looks only: nothing
# collides with them, and they are gone when their life is over.
#
#   Particles.burst(transform.position, count: 24, color: :gold, glow: true)
#   every(0.05) { Particles.burst(exhaust, count: 1, image: :smoke, gravity: -0.1, spread: 20, spin: 40) }
module Particles
  DEFAULT_COUNT = 12
  DEFAULT_SPEED = 3.0     # meters per second
  DEFAULT_SPREAD = 180.0  # degrees away from the direction: 180 goes everywhere
  DEFAULT_LIFE = 0.8      # seconds
  DEFAULT_SIZE = 0.12     # meters across
  NO_SPIN = 0.0
  WORLD_GRAVITY = 1.0     # falls like everything else; 0 floats, negative rises

  # glow: true makes them shine (sparks, magic); otherwise they cover what is behind (dust, smoke).
  # image: a picture from the game's images/ folder drawn on each particle (image: :smoke draws
  # images/smoke.png), tinted by the color; without one they are soft round dots. spin: degrees
  # per second each picture turns.
  def self.burst(position, count: DEFAULT_COUNT, color: :white, alpha: UI::OPAQUE, speed: DEFAULT_SPEED,
                 spread: DEFAULT_SPREAD, direction: Vector3.up, life: DEFAULT_LIFE, size: DEFAULT_SIZE,
                 gravity: WORLD_GRAVITY, glow: false, image: nil, spin: NO_SPIN)
    r, g, b = UI.rgb(color)
    __burst(position, direction, count.to_i, speed.to_f, spread.to_f, life.to_f, size.to_f, gravity.to_f,
            r, g, b, alpha.to_i, glow, image.to_s, spin.to_f)
  end
end
