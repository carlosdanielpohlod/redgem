# Spinning, bobbing collectible. Needs `trigger = true` in Blender.
class Coin < Behaviour
  SPIN_DEGREES_PER_SECOND = 180.0
  BOB_HEIGHT = 0.15
  BOB_SPEED = 3.0
  # What a collected coin bursts into (speed in m/s, size in meters, spin in degrees per second).
  SPARKS = { count: 24, color: :gold, speed: 3.5, size: 0.3, glow: true, image: :spark, spin: 240.0 }.freeze

  def start
    @base = transform.position
    @time = 0.0
  end

  def update(dt)
    @time += dt
    transform.rotate(Vector3.new(0, SPIN_DEGREES_PER_SECOND * dt, 0))
    transform.position = @base + Vector3.up * (Math.sin(@time * BOB_SPEED) * BOB_HEIGHT)
  end

  def on_trigger_enter(other)
    player = other.get_component(Player)
    return if player.nil? || player.ghost?  # another player's coin is theirs to report

    CoOp.instance.collected(name)
    collect
  end

  # Counts the coin and takes it out of the level (also when another player picked it up).
  def collect
    GameManager.instance.collect_coin(name)
    Particles.burst(transform.position, **SPARKS)
    destroy
  end
end
