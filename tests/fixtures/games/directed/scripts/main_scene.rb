# Sets the test scene up from Ruby: no object in it has a `script` property.
class MainScene < SceneScript
  WALKER_SPEED = 2.0

  def setup
    $scene_setups = ($scene_setups || 0) + 1
    $movers_started_before_setup = $mover_starts
    find!("Walker").add_component(Mover, speed: WALKER_SPEED)
    find!("Tuned").add_component(Mover)    # speed comes from the object's own property
    find!("Plain").add_component(Mover)    # ... or from the behaviour's default
  end
end
