# A spinning collectible. Needs trigger = true; the player collects it by touching it.
class Coin < Behaviour
  SPIN_DEGREES_PER_SECOND = 180.0

  def update(dt)
    transform.rotate(Vector3.new(0, SPIN_DEGREES_PER_SECOND * dt, 0))
  end

  def on_trigger_enter(other)
    player = other.get_component(Player)
    return unless player

    player.collect_coin
    destroy
  end
end
