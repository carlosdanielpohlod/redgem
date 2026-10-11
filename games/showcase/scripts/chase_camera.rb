# Camera behind the car, following where its nose points (asks the ArcadeCar on the target). Speed
# shows: the view widens, the picture streaks at its edges and the camera trembles a little.
# The mouse looks around: drag with a button held to orbit the car, turn the wheel to come closer
# or move away; a gamepad's right stick orbits too. Let go and the camera swings back behind the car.
# Settings: target (the car: the object or its name), distance, height (m), smoothing.
class ChaseCamera < Behaviour
  DEFAULT_DISTANCE = 7.5
  DEFAULT_HEIGHT = 2.2
  DEFAULT_SMOOTHING = 9.0
  LOOK_HEIGHT = 1.0
  MAX_BLEND = 1.0
  ORBIT_DEGREES_PER_PIXEL = 0.25
  STICK_DEGREES_PER_SECOND = 120.0   # how fast a gamepad's right stick swings the camera
  MIN_PITCH = -8.0            # degrees: how low the camera may go (never under the road)
  MAX_PITCH = 55.0
  RETURN_SPEED = 2.5          # how fast the camera swings back behind the car once the mouse lets go
  ZOOM_PER_NOTCH = 0.9        # meters per notch of the wheel
  MIN_DISTANCE = 3.5
  MAX_DISTANCE = 16.0
  ORBIT_BUTTONS = %i[left right].freeze
  DEGREES_PER_RADIAN = 180.0 / Math::PI
  # What speed does to the picture, from SLOW (nothing) to FAST (all of it), in m/s.
  SLOW = 9.0
  FAST = 28.0
  CALM_FOV = 52.0             # degrees, top to bottom
  FAST_FOV = 62.0
  FULL_STREAKS = 0.7          # of the engine's motion blur
  SHAKE = 0.035               # meters the camera trembles at full speed
  SHAKE_SPEED = 37.0          # how fast it trembles (radians per second)
  SHAKE_VERTICAL = 1.7        # the up-and-down tremble runs this much faster than the sideways one
  FOV_QUICKNESS = 3.0

  def start
    @target = target_object
    @car = @target.get_component(ArcadeCar)
    @distance = prop(:distance, DEFAULT_DISTANCE)
    @height = prop(:height, DEFAULT_HEIGHT)
    @smoothing = prop(:smoothing, DEFAULT_SMOOTHING)
    look_straight_ahead
    follow(MAX_BLEND) if @car
  end

  # Stops following, leaving the camera to whoever moves it next (photo views, cutscenes).
  def release
    @car = nil
    Camera.motion_blur = 0.0
  end

  def update(dt)
    return unless @car && @target.alive?

    look_around(dt)
    follow([@smoothing * dt, MAX_BLEND].min)
    feel_the_speed(dt)
  end

  private

  # The car to follow: the setting is the object itself, or its name (which must be right).
  def target_object
    target = prop(:target, "")
    target.is_a?(GameObject) ? target : find!(target)
  end

  def look_straight_ahead
    @orbit_yaw = 0.0    # degrees around the car, away from straight behind it
    @orbit_pitch = 0.0  # degrees above the usual view
    @fov = CALM_FOV
  end

  def orbiting? = ORBIT_BUTTONS.any? { |button| Input.mouse_down?(button) }

  # A gamepad's right stick, pushed away from its center.
  def stick = [Input.gamepad_axis(:right_x), Input.gamepad_axis(:right_y)]

  def look_around(dt)
    @distance = (@distance - Input.mouse_wheel * ZOOM_PER_NOTCH).clamp(MIN_DISTANCE, MAX_DISTANCE)
    return orbit(Input.mouse_dx * ORBIT_DEGREES_PER_PIXEL, Input.mouse_dy * ORBIT_DEGREES_PER_PIXEL) if orbiting?

    stick_x, stick_y = stick
    return swing_back(dt) if stick_x.zero? && stick_y.zero?

    orbit(stick_x * STICK_DEGREES_PER_SECOND * dt, stick_y * STICK_DEGREES_PER_SECOND * dt)
  end

  # Turns the camera around the car, by degrees: sideways and up.
  def orbit(sideways, upward)
    @orbit_yaw -= sideways
    @orbit_pitch = (@orbit_pitch + upward).clamp(MIN_PITCH, MAX_PITCH)
  end

  # Nobody is looking around: back behind the car, little by little.
  def swing_back(dt)
    ease = [RETURN_SPEED * dt, MAX_BLEND].min
    @orbit_yaw -= @orbit_yaw * ease
    @orbit_pitch -= @orbit_pitch * ease
  end

  # Where the camera sits: behind the car's nose, turned around it by the mouse.
  def seat
    pitch = @orbit_pitch / DEGREES_PER_RADIAN
    @target.transform.position + behind * (@distance * Math.cos(pitch)) +
      Vector3.up * (@height + @distance * Math.sin(pitch))
  end

  # The way from the car to the camera, flat on the road.
  def behind
    nose = @car.nose
    yaw = (Math.atan2(nose.x, nose.z) * DEGREES_PER_RADIAN + @orbit_yaw) / DEGREES_PER_RADIAN
    Vector3.new(-Math.sin(yaw), 0, -Math.cos(yaw))
  end

  # 0 when slow, 1 at full speed.
  def rush = ((@car.speed.abs - SLOW) / (FAST - SLOW)).clamp(0.0, 1.0)

  def feel_the_speed(dt)
    widen_the_view(dt)
    Camera.motion_blur = orbiting? ? 0.0 : FULL_STREAKS * rush
    tremble(SHAKE * rush) unless orbiting?
  end

  def widen_the_view(dt)
    @fov += (CALM_FOV + (FAST_FOV - CALM_FOV) * rush - @fov) * [FOV_QUICKNESS * dt, MAX_BLEND].min
    Camera.fov = @fov
  end

  # Shakes the camera by up to `size` meters, sideways and up and down.
  def tremble(size)
    return if size.zero?

    time = Time.elapsed * SHAKE_SPEED
    sideways = transform.right * (Math.sin(time) * size)
    transform.position = transform.position + sideways + Vector3.up * (Math.sin(time * SHAKE_VERTICAL) * size)
  end

  def follow(blend)
    # While the mouse is looking around the camera goes where it is told, without lagging.
    transform.position = orbiting? ? seat : transform.position.lerp(seat, blend)
    transform.look_at(@target.transform.position + Vector3.up * LOOK_HEIGHT)
  end
end
