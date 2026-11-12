module Camera
  # A cut through black (Camera.cut_to with a fade): a behaviour of the camera cut to. The picture
  # darkens over the first half of the fade, the game is seen through that camera from the darkest
  # frame on, and the picture comes back over the second half. It counts the game's unscaled time,
  # so it plays on while the game is paused. A newer cut, or Camera.main =, stops it at once.
  class Cut < Behaviour
    HALFWAY = 0.5
    WHOLE = 1.0
    SPAN = 2.0 # the darkness rises over half the fade and falls over the other half

    # How dark the picture is now: 0 (as it is) to 1 (black).
    attr_reader :darkness

    # Starts the cut over, `seconds` long. Returns it.
    def begin_fade(seconds)
      @seconds = seconds
      @elapsed = 0.0
      @darkness = 0.0
      @switched = false
      @playing = true
      self
    end

    def playing? = @playing && game_object.alive? && newest?

    def update(_dt)
      return unless playing?

      @elapsed += Time.unscaled_delta
      Camera.__choose(game_object, "Camera.cut_to") if crossed_halfway?
      @darkness = darkness_at(share)
      cover
      @playing = share < WHOLE
    end

    private

    def newest? = Camera.__cut.equal?(self)

    # How far through the fade it is, 0 to 1.
    def share = (@elapsed / @seconds).clamp(0.0, WHOLE)

    # Seen through the new camera from the frame the fade is halfway, once.
    def crossed_halfway?
      return false unless due_to_switch?

      @switched = true
    end

    def due_to_switch? = !@switched && share >= HALFWAY

    def darkness_at(share) = WHOLE - (SPAN * share - WHOLE).abs

    # Draws the black over the picture, as dark as the cut is now.
    def cover
      alpha = (@darkness * UI::OPAQUE).round
      UI.rect(x: 0, y: 0, width: UI.width, height: UI.height, color: :black, alpha: alpha) if alpha.positive?
    end
  end
end
