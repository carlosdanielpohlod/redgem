# The hero: WASD / arrows or a gamepad's left stick walk, Space or the gamepad's A jumps. Needs
# body = "character" on the object. In a game played together, each machine moves its own hero
# only (the others' are copies, see Party), and a dedicated server moves none.
class Player < Behaviour
  SPEED = 6.0
  JUMP_SPEED = 7.0
  FALL_LIMIT = -10.0         # below this height the hero starts again on its spawn point
  DEGREES_PER_RADIAN = 180.0 / Math::PI

  def update(dt)
    return unless walks?(dt)

    move(Vector3.new(Input.axis(:horizontal), 0, -Input.axis(:vertical)).clamped)
    Spawns.place(game_object) if transform.position.y < FALL_LIMIT
  end

  private

  # Not while a menu is open (dt is 0 then) or a conversation goes on, and only this machine's hero.
  def walks?(dt) = !dt.zero? && !Dialogue.busy? && mine?

  def mine? = get_component(NetworkSync)&.mine? != false

  def move(direction)
    character.move(direction * prop(:speed, SPEED))
    character.jump(prop(:jump_speed, JUMP_SPEED)) if jump_asked?
    face(direction) unless direction == Vector3.zero
  end

  def jump_asked? = Input.action_pressed?(:jump) && character.grounded?

  # Turns the hero so its forward (-Z) points the way it walks.
  def face(direction)
    yaw = Math.atan2(-direction.x, -direction.z) * DEGREES_PER_RADIAN
    transform.rotation = Vector3.new(0, yaw, 0)
  end
end
