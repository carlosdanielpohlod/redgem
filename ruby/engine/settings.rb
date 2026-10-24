# How the player chose the game to be shown and heard: the window, how sharp its picture is, the
# pace of its frames, how much the renderer does, the volume. A change takes effect at once;
# Settings.save keeps what is in use for the game's next runs. Until the player chooses, everything
# is what the game's game.json says (window_width, window_height, fullscreen, vsync, fps_cap,
# render_scale, quality).
#
#   Settings.fullscreen = true
#   Settings.resolution = [1920, 1080]      # one of Settings.resolutions
#   Settings.render_scale = 0.75            # the world drawn at 75% of the window's size
#   Settings.quality = :medium              # :low, :medium, :high, :ultra or :custom
#   Settings.save
module Settings
  WINDOW_MODES = %i[windowed fullscreen borderless].freeze
  QUALITIES = %i[low medium high ultra custom].freeze
  # What the engine's settings screen offers (anything in between can be set from a script).
  RENDER_SCALES = [0.5, 0.75, 1.0, 1.25, 1.5, 2.0].freeze
  FPS_CAPS = [0, 30, 60, 120, 144].freeze  # 0: no cap
  NO_CAP = 0
  # What a custom quality may say (see Settings.quality_details).
  QUALITY_DETAILS = %i[msaa antialias shadow_cascades shadow_distance scatter_density scatter_distance sky_detail
                       ambient_occlusion bloom].freeze

  # :windowed, :fullscreen (the monitor switched to the window's size) or :borderless (a window as
  # big as the monitor).
  def self.window_mode = read("window_mode").to_sym

  def self.window_mode=(mode)
    write("window_mode" => known(mode, WINDOW_MODES, "window mode"))
  end

  def self.fullscreen? = window_mode == :fullscreen

  def self.fullscreen=(wanted)
    self.window_mode = wanted ? :fullscreen : :windowed
  end

  # [width, height] of the window, in pixels.
  def self.resolution = [read("width"), read("height")]

  def self.resolution=(size)
    width, height = size
    write("width" => width.to_i, "height" => height.to_i)
  end

  # The sizes the window may take on its monitor, smallest first: [[width, height], ...]. None
  # where the game has no window of its own (a test, the editor).
  def self.resolutions = __modes

  # The share of the window's size the world is drawn at (0.5 to 2), stretched over the window.
  def self.render_scale = read("render_scale")

  def self.render_scale=(scale)
    write("render_scale" => scale.to_f)
  end

  def self.vsync = read("vsync")

  def self.vsync=(wanted)
    write("vsync" => wanted ? true : false)
  end

  # Frames a second at most; nil for no cap.
  def self.fps_cap
    cap = read("fps_cap")
    cap == NO_CAP ? nil : cap
  end

  def self.fps_cap=(cap)
    write("fps_cap" => (cap || NO_CAP).to_i)
  end

  # :low, :medium, :high, :ultra (everything the level asks for) or :custom (quality_details).
  def self.quality = read("quality").to_sym

  def self.quality=(quality)
    write("quality" => known(quality, QUALITIES, "quality"))
  end

  # What the custom quality allows: { msaa: 8, antialias: true, shadow_cascades: 4,
  # shadow_distance: 1.0, ... } (shares of what the level asks for are 0 to 1).
  def self.quality_details = read("custom_quality").transform_keys(&:to_sym)

  # Changes some of them, and makes the quality :custom.
  def self.quality_details=(details)
    details.each_key { |name| known(name, QUALITY_DETAILS, "quality detail") }
    write("quality" => "custom", "custom_quality" => details.transform_keys(&:to_s))
  end

  # The game's master volume, 0 (silent) to 1.
  def self.volume = read("volume")

  def self.volume=(volume)
    write("volume" => volume.to_f)
  end

  def self.read(key) = __read[key]

  def self.write(changes)
    __write(changes)
    nil
  end

  # The name of one of `names`, as a text; an ArgumentError for anything else.
  def self.known(name, names, what)
    return name.to_s if names.map(&:to_s).include?(name.to_s)

    raise ArgumentError, "unknown #{what} #{name.inspect} (#{names.join(", ")})"
  end
end
