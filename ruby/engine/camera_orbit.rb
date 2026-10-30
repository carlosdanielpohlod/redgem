module Camera
  # The camera orbiting an object or a point (Camera.orbit), looking at it. Options:
  #   distance:    meters from it
  #   yaw:         degrees around it: 0 is behind it (on its +Z side), 90 on its right; kept within
  #                -180..180, and eased the short way round (turn(720) ends where it started)
  #   pitch:       degrees above it (below it when negative)
  #   distances:   the range zooming keeps the distance in
  #   pitches:     the range turning keeps the pitch in (within -89..89)
  #   smoothing:   seconds the camera takes to close half the way to a new angle or distance
  #   input:       whether the player turns it (dragging with the right mouse button, or the
  #                right stick) and zooms it (the mouse wheel), also while the game is paused or
  #                slowed (it goes by Time.unscaled_delta)
  #   height:      meters above the object's origin (or the point) it orbits
  #   avoid_walls: whether it comes in front of a wall between it and what it orbits
  # Scripts turn and zoom it too: Camera.orbit(...).turn(15, 0), .zoom(-2).
  class Orbit
    DISTANCE = 6.0
    PITCH = 20.0
    DISTANCES = 1.0..30.0
    NEAREST = 0.1            # meters: nearer, the camera would sit on what it looks at (and never zoom out)
    PITCHES = -80.0..80.0
    STEEPEST = 89.0          # degrees: straight above or below, the camera would not know which way is up
    SMOOTHING = 0.08
    HEIGHT = 1.0
    MOUSE_DEGREES = 0.3      # degrees the camera turns per pixel the mouse is dragged
    STICK_DEGREES = 150.0    # degrees per second it turns with the stick pushed all the way
    ZOOM_SHARE = 0.1         # of the distance, per notch of the wheel
    DRAG_BUTTON = :right
    RADIANS = Math::PI / 180.0
    HALF_TURN = 180.0
    FULL_TURN = 360.0

    attr_reader :target, :yaw, :pitch, :distance

    def initialize(target, distance: DISTANCE, yaw: 0.0, pitch: PITCH, distances: DISTANCES, pitches: PITCHES,
                   smoothing: SMOOTHING, input: true, height: HEIGHT, avoid_walls: true)
      @target = target
      limit(distances, pitches)
      aim(yaw, pitch, distance)
      @smoothing = Camera.__number(smoothing, "Camera.orbit smoothing", 0.0)
      @height = Camera.__number(height, "Camera.orbit height", -LARGEST)
      @input = input
      @avoid_walls = avoid_walls
    end

    # Whether the object orbited is gone (a point never is).
    def lost? = orbits_object? && !@target.alive?

    # Turns the camera around by degrees, kept within its pitches.
    def turn(yaw, pitch)
      @yaw = Orbit.wrap(@yaw + Camera.__number(yaw, "Orbit#turn yaw", -LARGEST))
      @pitch = Camera.__clamp(@pitch + Camera.__number(pitch, "Orbit#turn pitch", -LARGEST), @pitches)
    end

    # Comes nearer (negative meters) or goes further, kept within its distances.
    def zoom(meters)
      @distance = Camera.__clamp(@distance + Camera.__number(meters, "Orbit#zoom", -LARGEST), @distances)
    end

    # The player turns it by the frame's real seconds, so it turns while the game is paused or slowed.
    def update(transform, _dt, real_dt)
      read_input(real_dt) if reads_input?(real_dt)
      ease(Camera.__share(@smoothing, real_dt))
      seat = center + Orbit.direction(@shown[:yaw], @shown[:pitch]) * @shown[:distance]
      transform.position = @avoid_walls ? Camera.__clear_of_walls(center, seat, seen_through) : seat
      Camera.__look(transform, center)
    end

    # An angle in degrees as the same angle within -180..180.
    def self.wrap(degrees) = (degrees + HALF_TURN) % FULL_TURN - HALF_TURN

    # The way from what is orbited to the camera, at those angles in degrees.
    def self.direction(yaw, pitch)
      flat = Math.cos(pitch * RADIANS)
      Vector3.new(Math.sin(yaw * RADIANS) * flat, Math.sin(pitch * RADIANS), Math.cos(yaw * RADIANS) * flat)
    end

    private

    def limit(distances, pitches)
      @distances = Camera.__range(distances, "Camera.orbit distances", NEAREST, LARGEST)
      @pitches = Camera.__range(pitches, "Camera.orbit pitches", -STEEPEST, STEEPEST)
    end

    def aim(yaw, pitch, distance)
      @yaw = Orbit.wrap(Camera.__number(yaw, "Camera.orbit yaw", -LARGEST))
      @pitch = Camera.__clamp(Camera.__number(pitch, "Camera.orbit pitch", -STEEPEST, STEEPEST), @pitches)
      @distance = Camera.__clamp(Camera.__number(distance, "Camera.orbit distance", 0.0), @distances)
    end

    def reads_input?(real_dt) = @input && real_dt.positive?

    def orbits_object? = @target.is_a?(GameObject)

    def center
      point = orbits_object? ? @target.transform.position : @target
      point + Vector3.up * @height
    end

    def seen_through = orbits_object? ? @target : nil

    # The angles and distance shown close in on those asked for (they start there).
    def ease(share)
      @shown ||= { yaw: @yaw, pitch: @pitch, distance: @distance }
      @shown[:yaw] = Orbit.wrap(@shown[:yaw] + Orbit.wrap(@yaw - @shown[:yaw]) * share)
      @shown[:pitch] += (@pitch - @shown[:pitch]) * share
      @shown[:distance] += (@distance - @shown[:distance]) * share
    end

    def read_input(dt)
      dragged_x, dragged_y = dragged
      turn(-(dragged_x + Input.gamepad_axis(:right_x) * STICK_DEGREES * dt),
           dragged_y + Input.gamepad_axis(:right_y) * STICK_DEGREES * dt)
      zoom(-Input.mouse_wheel * ZOOM_SHARE * @distance)
    end

    # Degrees the mouse turns the camera by this frame, across and up: none unless it is dragged.
    def dragged
      return [0.0, 0.0] unless Input.mouse_down?(DRAG_BUTTON)

      [Input.mouse_dx * MOUSE_DEGREES, Input.mouse_dy * MOUSE_DEGREES]
    end
  end
end
