# A trigger that destroys itself when the Hero touches it.
class CoinProbe < Behaviour
  def on_trigger_enter(other)
    $coin_touched_by = other.name
    destroy if other.name == "Hero"
  end

  def on_destroy
    $coin_destroyed = true
  end
end
