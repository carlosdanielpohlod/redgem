# Slow cinematic orbit around the crossroad. Blender props: radius, height, speed (rad/s), look_height.
class OrbitCamera < Behaviour
  DEFAULT_RADIUS = 46.0
  DEFAULT_HEIGHT = 22.0
  DEFAULT_SPEED = 0.08
  DEFAULT_LOOK_HEIGHT = 2.0
  # The height gently rises and falls so the view does not feel static.
  BOB_AMOUNT = 6.0
  BOB_SPEED = 0.23

  def start
    @radius = prop(:radius, DEFAULT_RADIUS)
    @height = prop(:height, DEFAULT_HEIGHT)
    @speed = prop(:speed, DEFAULT_SPEED)
    @target = Vector3.up * prop(:look_height, DEFAULT_LOOK_HEIGHT)
    position = transform.position
    @angle = Math.atan2(position.z, position.x)
    @time = 0.0
  end

  def update(dt)
    @time += dt
    @angle += @speed * dt
    height = @height + Math.sin(@time * BOB_SPEED) * BOB_AMOUNT
    transform.position = Vector3.new(Math.cos(@angle) * @radius, height, Math.sin(@angle) * @radius)
    transform.look_at(@target)
  end
end
