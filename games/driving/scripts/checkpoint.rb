# Trigger gate. Blender prop `order` says when it must be crossed in a lap.
class Checkpoint < Behaviour
  def order = prop(:order).to_i

  def on_trigger_enter(other)
    return unless other.get_component(Car)

    RaceManager.instance&.reach(order)
  end
end
