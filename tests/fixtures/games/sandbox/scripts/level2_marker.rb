# Lives in the sandbox's second scene: proves the scene loaded and its scripts run.
class Level2Marker < Behaviour
  def start
    $level2_started = true
  end
end
