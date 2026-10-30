module Camera
  # What moves the camera for the helpers (Camera.follow, orbit and shake): a behaviour of the
  # camera's own, put on it the first time one of them is called. Each frame it moves the camera
  # after what it follows or orbits, and gives the renderer its shake: a shake only moves what the
  # camera shows, never the camera, so nothing a script reads or writes of it ever drifts. A camera
  # that is no longer the main one stops, and lets its shake go.
  class Rig < Behaviour
    attr_accessor :motion

    def shake = @shake ||= Shake.new

    # Whether this is the rig of that camera, still there.
    def on?(camera) = game_object.alive? && game_object == camera

    def update(dt)
      return let_go unless main?

      @motion = nil if @motion&.lost?
      @motion&.update(transform, dt, Time.unscaled_delta)
      shake.advance(dt)
      show_shake
    end

    private

    def main? = game_object == Camera.main

    def let_go
      @motion = nil
      shake.stop
      show_shake
    end

    # Hands the renderer the shake as it is now (and once more when it is over, to put it away).
    def show_shake
      return if stays_still?

      Camera.__shake_by(game_object, *shake.offset)
      @still = !shake.active?
    end

    def stays_still? = @still && !shake.active?
  end
end
