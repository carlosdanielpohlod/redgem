# How QuestMarkers (see quest_markers.rb) works out its marks, and draws them: each objective under
# way of the tracked quest that has a place is seen through the main camera; one the screen shows
# is a marker there, another a pointer at the edge on the way to it (to its side, and down, for one
# behind the camera).
module QuestMarkers
  class << self
    private

    def shown? = enabled && !Network.server? && !Camera.main.nil?

    def current_marks(hero)
      quest = Quests.tracked
      return [] unless quest

      quest.objectives.reject(&:done?).filter_map { |objective| mark_of(objective, hero) }
    end

    def mark_of(objective, hero)
      point = place_of(objective)
      point && mark_at(objective, point, hero.distance_to(point))
    end

    # Where an objective happens: where the game says, or the object it names; nil when neither is.
    def place_of(objective)
      found = @locate&.call(objective) || named_place(objective)
      found.respond_to?(:transform) ? found.transform.position : found
    end

    def named_place(objective)
      name = PLACED.key?(objective.kind) ? objective.__send__(PLACED.fetch(objective.kind)) : nil
      name && Scene.find(name)
    end

    # A marker where the camera shows the place (in the middle when it is too near to show), else a
    # pointer at the edge.
    def mark_at(objective, point, meters)
      spot = screen_spot(point)
      seen = on_screen?(spot, UI.width, UI.height)
      return Mark.new(objective: objective, x: spot[0], y: spot[1], meters: meters) if seen

      direction = spot ? toward(spot, UI.width, UI.height) : behind(point)
      x, y = edge_point(direction, UI.width, UI.height)
      Mark.new(objective: objective, x: x, y: y, meters: meters, pointer: true, direction: direction)
    end

    def screen_spot(point)
      spot = Camera.world_to_screen(point + Vector3.up * @style[:height])
      too_near?(spot, point) ? [UI.width * HALF, UI.height * HALF] : spot
    end

    # The way from the middle of a screen to a point on (or past) it, as [x, y] of length 1.
    def toward(spot, width, height) = unit(spot[0] - width * HALF, spot[1] - height * HALF)

    # Whether a point the camera does not show is in front of it all the same: nearer than its near
    # plane (it is shown in the middle of the screen).
    def too_near?(spot, point)
      camera = Camera.main.transform
      spot.nil? && (point - camera.position).dot(camera.forward).positive?
    end

    # The way to a point behind the camera: to its side, and down.
    def behind(point)
      camera = Camera.main.transform
      offset = point - camera.position
      unit(offset.dot(camera.right), offset.length * HALF)
    end

    # How far a pointer may sit from the middle of a side `length` long, `margin` inside its ends.
    def room(length, margin) = [length * HALF - margin, 0].max

    def unit(across, down)
      length = Math.sqrt(across * across + down * down)
      length < NEAR ? DOWN : [across / length, down / length]
    end

    def paint(mark)
      return @draw.call(mark) if @draw

      size = mark.pointer ? @style[:pointer_size] : @style[:size]
      @style[:image] ? picture(mark, size) : dots(mark, size)
      label(mark, size) if @style[:distance]
    end

    def dots(mark, size)
      dot(mark.x, mark.y, size + RING, RING_COLOR)
      dot(mark.x, mark.y, size, @style[:color])
      tip(mark, size) if mark.pointer
    end

    # (On the pointer's side toward its place.)
    def tip(mark, size)
      reach = (size + TIP) * HALF
      dot(mark.x + mark.direction[0] * reach, mark.y + mark.direction[1] * reach, TIP, @style[:color])
    end

    def picture(mark, size)
      UI.image(@style[:image], x: mark.x - size * HALF, y: mark.y - size * HALF, width: size, height: size)
    end

    def label(mark, size)
      text = I18n.t(DISTANCE, meters: mark.meters.round)
      UI.text(text, x: mark.x, y: mark.y + size * HALF + LABEL_GAP, size: LABEL_SIZE, align: :center)
    end

    def dot(x, y, size, color)
      UI.rect(x: x - size * HALF, y: y - size * HALF, width: size, height: size, color: color, radius: size * HALF)
    end
  end
end
