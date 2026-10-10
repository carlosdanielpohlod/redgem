# Records everything that enters this trigger.
class Sensor < Behaviour
  def start
    $sensor_hits = []
  end

  def on_trigger_enter(other)
    $sensor_hits << other.name
  end
end
