# Spawns bullets from inside update (the behaviour list grows while it is being walked).
class Spawner < Behaviour
  def update(_dt)
    return unless $spawner_on

    Scene.spawn("Bullet", position: transform.position + Vector3.up * 3)
  end
end
