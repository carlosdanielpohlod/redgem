# Small things that fly and fade: sparks, dust, smoke, flames, confetti. They are looks only:
# nothing collides with them, and they are gone when their life is over.
#
#   Particles.burst(transform.position, count: 24, color: :gold, glow: true)
#   every(0.05) { Particles.burst(exhaust, count: 1, image: :smoke, gravity: -0.1, spread: 20, spin: 40) }
#   Particles.burst(torch, count: 1, image: :flames, frames: [8, 4], fps: 24, glow: true,
#                   size: [0.4, 0.6, 0.2], color: [[255, 220, 150], [255, 120, 40], [90, 30, 20]])
module Particles
  DEFAULT_COUNT = 12
  DEFAULT_SPEED = 3.0     # meters per second
  DEFAULT_SPREAD = 180.0  # degrees away from the direction: 180 goes everywhere
  DEFAULT_LIFE = 0.8      # seconds
  DEFAULT_SIZE = 0.12     # meters across
  NO_SPIN = 0.0
  WORLD_GRAVITY = 1.0     # falls like everything else; 0 floats, negative rises
  HARD_EDGE = 0.0         # meters a particle fades over where it meets the ground: none
  ONE_PICTURE = [1, 1].freeze  # columns and rows of an image that is not a sprite sheet
  OVER_LIFE = 0.0         # frames per second that show a sprite sheet once over the life

  # glow: true makes them shine (sparks, magic); otherwise they cover what is behind (dust, smoke).
  # image: a picture from the game's images/ folder drawn on each particle (image: :smoke draws
  # images/smoke.png), tinted by the color; without one they are soft round dots. spin: degrees
  # per second each picture turns. size, color and alpha may each be a list of up to 4 values the
  # particle goes through over its life (see docs/scripting/gameplay.md).
  def self.burst(position, count: DEFAULT_COUNT, color: :white, alpha: UI::OPAQUE, speed: DEFAULT_SPEED,
                 spread: DEFAULT_SPREAD, direction: Vector3.up, life: DEFAULT_LIFE, size: DEFAULT_SIZE,
                 gravity: WORLD_GRAVITY, glow: false, lit: false, soft: HARD_EDGE, image: nil, frames: ONE_PICTURE,
                 fps: OVER_LIFE, spin: NO_SPIN)
    columns, rows = frames
    __burst(position, direction, count.to_i, speed.to_f, spread.to_f, life.to_f, gravity.to_f, spin.to_f,
            numbers(size), colors(color), numbers(alpha), glow, lit, soft.to_f, image.to_s, columns.to_i, rows.to_i,
            fps.to_f)
  end

  def self.numbers(value) = value.is_a?(Array) ? value.map(&:to_f) : [value.to_f]

  # One color (:gold, [255, 200, 0]) or a list of them.
  def self.colors(color) = list_of_colors?(color) ? color.map { |each| UI.rgb(each) } : [UI.rgb(color)]

  def self.list_of_colors?(color) = color.is_a?(Array) && !color.first.is_a?(Numeric)
end
