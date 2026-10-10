# Moves along X at its speed. Uses a plain class from another script, which loads on first use.
class Mover < Behaviour
  DEFAULT_SPEED = 1.0

  attr_reader :speed

  def start
    $mover_starts = ($mover_starts || 0) + 1
    @speed = prop(:speed, DEFAULT_SPEED)
  end

  def update(dt)
    transform.translate(Vector3.new(Stride.length(@speed, dt), 0, 0))
  end
end
