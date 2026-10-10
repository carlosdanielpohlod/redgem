# Spawned from the Bullet prefab: counts how many copies started.
class Bullet < Behaviour
  def start
    $bullet_starts = ($bullet_starts || 0) + 1
  end
end
