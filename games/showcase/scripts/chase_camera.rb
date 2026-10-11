# Camera behind the car, following where its nose points (asks the ArcadeCar on the target). Speed
# shows: the view widens, the picture streaks at its edges and the camera trembles a little.
# The mouse looks around: drag with a button held to orbit the car, turn the wheel to come closer
# or move away. Let go and the camera swings back behind the car.
# Settings: target (the car: the object or its name), distance, height (m), smoothing.
class ChaseCamera < Behaviour
  DEFAULT_DISTANCE = 7.5
  DEFAULT_HEIGHT = 2.2
  DEFAULT_SMOOTHING = 9.0
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
    target = prop(:target, "")
    @target = target.is_a?(GameObject) ? target : find(target)
    @car = @target&.get_component(ArcadeCar)
    @distance = prop(:distance, DEFAULT_DISTANCE)
    @height = prop(:height, DEFAULT_HEIGHT)
    @smoothing = prop(:smoothing, DEFAULT_SMOOTHING)
    @orbit_yaw = 0.0    # degrees around the car, away from straight behind it
    @orbit_pitch = 0.0  # degrees above the usual view
    @fov = CALM_FOV
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

  # 0 when slow, 1 at full speed.
  def rush = ((@car.speed.abs - SLOW) / (FAST - SLOW)).clamp(0.0, 1.0)

  def feel_the_speed(dt)
    @fov += (CALM_FOV + (FAST_FOV - CALM_FOV) * rush - @fov) * [FOV_QUICKNESS * dt, MAX_BLEND].min
    Camera.fov = @fov
    Camera.motion_blur = orbiting? ? 0.0 : FULL_STREAKS * rush
    tremble = SHAKE * rush
    return if tremble.zero? || orbiting?

    time = Time.elapsed * SHAKE_SPEED
    transform.position = transform.position +
                         transform.right * (Math.sin(time) * tremble) +
                         Vector3.up * (Math.sin(time * SHAKE_VERTICAL) * tremble)
  end

  def follow(blend)
    # While the mouse is looking around the camera goes where it is told, without lagging.
    transform.position = orbiting? ? seat : transform.position.lerp(seat, blend)
    transform.look_at(@target.transform.position + Vector3.up * LOOK_HEIGHT)
  end
end
