# Camera behind the car, following where its nose points (asks the ArcadeCar on the target).
# The mouse looks around: drag with a button held to orbit the car, turn the wheel to come closer
# or move away. Let go and the camera swings back behind the car.
# Settings: target (the car: the object or its name), distance, height (m), smoothing.
class ChaseCamera < Behaviour
  DEFAULT_DISTANCE = 7.5
  DEFAULT_HEIGHT = 2.2
  DEFAULT_SMOOTHING = 5.0
  LOOK_HEIGHT = 1.0
  MAX_BLEND = 1.0
  ORBIT_DEGREES_PER_PIXEL = 0.25
  MIN_PITCH = -8.0            # degrees: how low the camera may go (never under the road)
  MAX_PITCH = 55.0
  RETURN_SPEED = 2.5          # how fast the camera swings back behind the car once the mouse lets go
  ZOOM_PER_NOTCH = 0.9        # meters per notch of the wheel
  MIN_DISTANCE = 3.5
  MAX_DISTANCE = 16.0
  ORBIT_BUTTONS = [:left, :right].freeze
  DEGREES_PER_RADIAN = 180.0 / Math::PI
  HINT_X = 24
  HINT_Y = 90
  HINT_SIZE = 20

  def start
    target = prop(:target, "")
    @target = target.is_a?(GameObject) ? target : find(target)
    @car = @target&.get_component(ArcadeCar)
    @distance = prop(:distance, DEFAULT_DISTANCE)
    @height = prop(:height, DEFAULT_HEIGHT)
    @smoothing = prop(:smoothing, DEFAULT_SMOOTHING)
    @orbit_yaw = 0.0    # degrees around the car, away from straight behind it
    @orbit_pitch = 0.0  # degrees above the usual view
    follow(MAX_BLEND) if @car
  end

  # Stops following, leaving the camera to whoever moves it next (photo views, cutscenes).
  def release
    @car = nil
  end

  def update(dt)
    return unless @car && @target.alive?

    look_around(dt)
    follow([@smoothing * dt, MAX_BLEND].min)
    UI.text("Mouse: drag to look around, wheel to zoom", x: HINT_X, y: HINT_Y, size: HINT_SIZE)
  end

  private

  def orbiting? = ORBIT_BUTTONS.any? { |button| Input.mouse_down?(button) }

  def look_around(dt)
    @distance = (@distance - Input.mouse_wheel * ZOOM_PER_NOTCH).clamp(MIN_DISTANCE, MAX_DISTANCE)
    if orbiting?
      @orbit_yaw -= Input.mouse_dx * ORBIT_DEGREES_PER_PIXEL
      @orbit_pitch = (@orbit_pitch + Input.mouse_dy * ORBIT_DEGREES_PER_PIXEL).clamp(MIN_PITCH, MAX_PITCH)
    else
      ease = [RETURN_SPEED * dt, MAX_BLEND].min
      @orbit_yaw -= @orbit_yaw * ease
      @orbit_pitch -= @orbit_pitch * ease
    end
  end

  # Where the camera sits: behind the car's nose, turned around it by the mouse.
  def seat
    nose = @car.nose
    yaw = (Math.atan2(nose.x, nose.z) * DEGREES_PER_RADIAN + @orbit_yaw) / DEGREES_PER_RADIAN
    pitch = @orbit_pitch / DEGREES_PER_RADIAN
    back = Vector3.new(-Math.sin(yaw), 0, -Math.cos(yaw))
    @target.transform.position + back * (@distance * Math.cos(pitch)) +
      Vector3.up * (@height + @distance * Math.sin(pitch))
  end

  def follow(blend)
    # While the mouse is looking around the camera goes where it is told, without lagging.
    transform.position = orbiting? ? seat : transform.position.lerp(seat, blend)
    transform.look_at(@target.transform.position + Vector3.up * LOOK_HEIGHT)
  end
end
