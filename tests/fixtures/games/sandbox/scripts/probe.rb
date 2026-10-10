# Records what happens to the Hero so the integration tests can inspect it.
class Probe < Behaviour
  DEFAULT_SPEED = 3.0
  JUMP_SPEED = 8.0
  DEGREES_PER_RADIAN = 180.0 / Math::PI

  def start
    $probe = { started: true, triggers: [], collisions: [], collision_exits: [], updates: 0, fixed_updates: 0 }
  end

  def update(_dt)
    $probe[:updates] += 1
    direction = Vector3.new(Input.axis(:horizontal), 0, -Input.axis(:vertical))
    character.move(direction * prop(:speed, DEFAULT_SPEED))
    character.jump(JUMP_SPEED) if Input.action_pressed?(:jump)
    # Facing the movement direction writes the rotation every frame, like a real player script.
    transform.rotation = Vector3.new(0, Math.atan2(-direction.x, -direction.z) * DEGREES_PER_RADIAN, 0) \
      unless direction == Vector3.zero
  end

  def fixed_update(_dt)
    $probe[:fixed_updates] += 1
  end

  def on_trigger_enter(other)
    $probe[:triggers] << "enter:#{other.name}"
  end

  def on_trigger_exit(other)
    $probe[:triggers] << "exit:#{other.name}"
  end

  def on_collision_enter(other)
    $probe[:collisions] << other.name
  end

  def on_collision_exit(other)
    $probe[:collision_exits] << other.name
  end
end
