module Camera
  # The camera following an object (Camera.follow). Options:
  #   offset:      where the camera sits from the object, in meters (behind and above it by default)
  #   relative:    false: the offset is in the world's axes; true: in the object's own (its +Z is
  #                behind it), so the camera turns with it
  #   smoothing:   seconds the camera takes to close half the way to where it should be; 0 sticks
  #   look_at:     whether it turns to look at the object
  #   look_height: meters above the object's origin it looks at (and sees walls from)
  #   avoid_walls: whether it comes in front of a wall between it and the object (Physics.raycast)
  #   dead_zone:   meters the object moves before the camera goes after it
  # It starts from where the camera is, and closes in as smoothing says.
  class Follow
    OFFSET = Vector3.new(0, 3, 6)
    SMOOTHING = 0.15
    LOOK_HEIGHT = 1.0

    attr_reader :target

    def initialize(target, offset: OFFSET, relative: false, smoothing: SMOOTHING, look_at: true,
                   look_height: LOOK_HEIGHT, avoid_walls: true, dead_zone: 0.0)
      @target = target
      @offset = Camera.__vector(offset, "Camera.follow offset")
      @smoothing = Camera.__number(smoothing, "Camera.follow smoothing", 0.0)
      @look_height = Camera.__number(look_height, "Camera.follow look_height", -LARGEST)
      @dead_zone = Camera.__number(dead_zone, "Camera.follow dead_zone", 0.0)
      @relative = relative
      @look_at = look_at
      @avoid_walls = avoid_walls
    end

    # Whether the object followed is gone.
    def lost? = !@target.alive?

    def update(transform, dt)
      @focus = focus
      goal = @focus + world_offset
      @position = (@position || transform.position).lerp(goal, Camera.__share(@smoothing, dt))
      transform.position = @avoid_walls ? Camera.__clear_of_walls(look_point, @position, @target) : @position
      transform.look_at(look_point) if @look_at
    end

    private

    # The point followed: the object, or as far behind it as the dead zone lets it lag.
    def focus
      point = @target.transform.position
      return point if @focus.nil? || @dead_zone.zero?

      away = point - @focus
      away.length <= @dead_zone ? @focus : point - away.normalized * @dead_zone
    end

    def look_point = @focus + Vector3.up * @look_height

    def world_offset
      return @offset unless @relative

      turned = @target.transform
      turned.right * @offset.x + turned.up * @offset.y - turned.forward * @offset.z
    end
  end
end
