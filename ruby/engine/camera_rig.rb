module Camera
  # What moves the camera for the helpers (Camera.follow, orbit and shake): a behaviour of the
  # camera's own, put on it the first time one of them is called. Each frame it takes back the
  # last frame's shake, moves the camera after what it follows or orbits, then shakes it again: a
  # shake is only ever an offset, so the camera never drifts from where it should be.
  class Rig < Behaviour
    PLACE = 0..2 # where a pose (see Camera.__pose) keeps the camera's place
    TURN = 3..6  # and its turn
    attr_accessor :motion

    def shake = @shake ||= Shake.new

    # Whether this is the rig of that camera, still there.
    def on?(camera) = game_object.alive? && game_object == camera

    def update(dt)
      settle
      @motion = nil if @motion&.lost?
      @motion&.update(transform, dt)
      shake.advance(dt)
      shake_camera if shake.active?
    end

    private

    # Puts the camera back where the last frame's shake took it from, its place and its turn each
    # unless something else (a script of the game's) moved it since: then that is where it should be.
    def settle
      return if @shaken.nil?

      Camera.__set_pose(game_object, unshaken_parts(Camera.__pose(game_object)))
      @shaken = nil
    end

    # Each part of the pose (place, turn) as before the shake when the shake left it as it was.
    def unshaken_parts(pose)
      [PLACE, TURN].flat_map { |part| pose[part] == @shaken[part] ? @unshaken[part] : pose[part] }
    end

    def shake_camera
      @unshaken = Camera.__pose(game_object)
      shake.apply(transform)
      @shaken = Camera.__pose(game_object)
    end
  end
end
