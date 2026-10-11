# Spinning, bobbing collectible. Needs `trigger = true` in Blender.
class Coin < Behaviour
  SPIN_DEGREES_PER_SECOND = 180.0
  BOB_HEIGHT = 0.15
  BOB_SPEED = 3.0
  SPARKS = 24
  SPARK_SPEED = 3.5
  SPARK_SIZE = 0.14

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
    return unless other.get_component(Player)

    GameManager.instance&.collect_coin
    Particles.burst(transform.position, count: SPARKS, color: :gold, speed: SPARK_SPEED, size: SPARK_SIZE, glow: true)
    destroy
  end
end
