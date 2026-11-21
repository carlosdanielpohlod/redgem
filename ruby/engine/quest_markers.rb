# Where the tracked quest's objectives happen (Quests.tracked), shown on this machine's screen: a
# marker over each place the camera sees, with how far it is, and a pointer at the edge of the
# screen toward each place it does not (one behind the camera too). The QuestTracker of this
# machine's hero draws them every frame, over the game. An objective's place is the object of the
# scene it names: a go_to's place, a talk_to's speaker, a deliver's recipient; a game may say where
# any objective happens instead (locate). Nothing shows while the game is frozen (a menu is open),
# nor on a dedicated server. The quests are the party's (the host's), but each machine tracks and
# shows its own. Experimental.
#
#   QuestMarkers.enabled = false                         # none: the game shows its own, or nothing
#   QuestMarkers.style(color: :ruby, size: 24, distance: false)
#   QuestMarkers.locate { |objective| Vector3.new(4, 0, 9) if objective.target == "flour" }
#   QuestMarkers.draw { |mark| UI.image(:arrow, x: mark.x - 16, y: mark.y - 16, width: 32) }
#
# What QuestMarkers.locate and draw register lasts until the next level is loaded; enabled and the
# style last for the whole run.
module QuestMarkers
  # color: of a marker and a pointer; size: units across a marker; pointer_size: across a pointer;
  # margin: units between a pointer and the edge of the screen; height: meters above a place a
  # marker floats; distance: whether the meters to it show under it; image: a picture of images/
  # drawn in the place of the dots (nil: dots).
  STYLE = { color: [255, 196, 32], size: 18, pointer_size: 26, margin: 48, height: 1.5, distance: true,
            image: nil }.freeze
  PLACED = { go_to: :target, talk_to: :target, deliver: :recipient }.freeze   # the name of each one's place
  DISTANCE = "game.quest.distance".freeze
  RING = 4              # units of the dark ring around a dot
  RING_COLOR = :black
  TIP = 10              # units across the dot beside a pointer, on its place's side
  LABEL_SIZE = 18
  LABEL_GAP = 4         # units between a dot and its distance
  HALF = 0.5
  NEAR = 1e-3           # a direction shorter than this has none
  DOWN = [0.0, 1.0].freeze

  # One place shown: the objective; where on the screen it is drawn (the interface's units); whether
  # it is a pointer (the camera does not show the place) and toward where on the screen ([x, y],
  # length 1; nil for a marker); and the meters from the hero to it.
  class Mark
    attr_reader :objective, :x, :y, :pointer, :direction, :meters

    def initialize(objective:, x:, y:, meters:, pointer: false, direction: nil)
      @objective = objective
      @x = x
      @y = y
      @meters = meters
      @pointer = pointer
      @direction = direction
    end
  end

  @enabled = true
  @style = STYLE

  class << self
    attr_reader :enabled

    # What it shows now: a Mark for each objective under way of the tracked quest that has a place.
    def marks = @marks || []

    def enabled=(shown)
      @enabled = shown ? true : false
    end

    # Changes some of the looks (the keys of STYLE); returns them all. With nothing, they are as they are.
    def style(**changes)
      unknown = changes.keys - STYLE.keys
      raise ArgumentError, "QuestMarkers.style takes #{STYLE.keys.inspect}, not #{unknown.inspect}" if unknown.any?

      @style = @style.merge(changes)
    end

    # The looks back to the engine's.
    def reset_style = @style = STYLE

    # |objective| where it happens: an object, a Vector3, or nil for where the engine looks (the
    # object it names). Without a block: the engine's places only.
    def locate(&block)
      @locate = block
    end

    # |mark| draws a mark as the game wants, in the place of the engine's dots. Without a block:
    # the engine's.
    def draw(&block)
      @draw = block
    end

    # Where a pointer sits on a screen `width` by `height` toward a direction ([x, y] from the middle,
    # y down): on the way to it, `margin` inside the edge.
    def edge_point(direction, width, height, margin = @style[:margin])
      across, down = direction
      reach = [room(width, margin) / [across.abs, NEAR].max, room(height, margin) / [down.abs, NEAR].max].min
      [width * HALF + across * reach, height * HALF + down * reach]
    end

    # Whether a point ([x, y], nil behind the camera) is on a screen `width` by `height`.
    def on_screen?(spot, width, height) = !spot.nil? && spot[0].between?(0, width) && spot[1].between?(0, height)

    # Called by this machine's QuestTracker every frame, with its hero's position. They are drawn
    # once a frame, and not while the game is frozen (dt is 0).
    def __update(hero, dt)
      @marks = shown? ? current_marks(hero) : []
      return if dt.zero? || @painted == Time.frame

      @painted = Time.frame
      @marks.each { |mark| paint(mark) }
    end

    def __scene_reset
      @locate = nil
      @draw = nil
      @marks = []
      @painted = nil
    end
  end
end
