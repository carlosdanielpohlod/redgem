# The menu's settings screen (see GameMenu): how the game is shown (window mode, resolution,
# render scale, vsync, frame cap, quality) and heard (master volume), as the Settings module has
# them. Each change takes effect at once; leaving the screen (its Back button or Esc) keeps them
# for the game's next runs and goes back to the screen it was opened from.
class GameMenu < Behaviour
  class SettingsScreen < Screen
    ROW_WIDTH = 560
    ROW_HEIGHT = 40
    ROW_GAP = 8
    CAPTION_SIZE = 22
    CAPTION_HEIGHT = 30        # pixels a section's caption takes above its rows
    CAPTION_COLOR = :gold
    BUTTONS_SHARE = 0.8        # where its Back button sits, as a share of the screen's height
    PERCENT = 100
    VOLUME_STEP = 0.05
    VIDEO_ROWS = %i[window_mode resolution render_scale vsync fps_cap quality].freeze
    AUDIO_ROWS = %i[volume].freeze
    SECTIONS = [[-> { I18n.t("game.settings.video") }, VIDEO_ROWS],
                [-> { I18n.t("game.settings.audio") }, AUDIO_ROWS]].freeze
    # What each window mode and quality is called, in the language in use.
    MODE_NAMES = { windowed: -> { I18n.t("game.settings.windowed") },
                   fullscreen: -> { I18n.t("game.settings.fullscreen") },
                   borderless: -> { I18n.t("game.settings.borderless") } }.freeze
    QUALITY_NAMES = { low: -> { I18n.t("game.settings.quality_low") },
                      medium: -> { I18n.t("game.settings.quality_medium") },
                      high: -> { I18n.t("game.settings.quality_high") },
                      ultra: -> { I18n.t("game.settings.quality_ultra") },
                      custom: -> { I18n.t("game.settings.quality_custom") } }.freeze

    # The name of the screen it goes back to.
    attr_accessor :from

    def initialize(menu)
      super(heading: -> { I18n.t("game.settings.heading") }, back: -> { leave(menu) })
      @menu = menu
      @from = :title
    end

    def options = [[I18n.t("game.menu.back"), back]]
    def rows = VIDEO_ROWS.size + AUDIO_ROWS.size
    def space_above = rows * row_step + SECTIONS.size * @menu.scaled(CAPTION_HEIGHT)
    def first_button_share = BUTTONS_SHARE

    def draw(first_button)
      y = first_button[1] - space_above
      first = 0
      SECTIONS.each do |caption, names|
        y = section(caption, names, first, y)
        first += names.size
      end
    end

    private

    # Keeps the settings, and goes back where it was opened from (into the game, from nowhere).
    def leave(menu)
      Settings.save
      @from ? menu.show(@from) : menu.resume
    end

    # A caption, then its rows (`first` is the first one's place in the focus). Returns where the
    # next section starts.
    def section(caption, names, first, y)
      UI.text(caption.call, x: row_x, y: y, size: @menu.scaled(CAPTION_SIZE), color: CAPTION_COLOR)
      y += @menu.scaled(CAPTION_HEIGHT)
      names.each_with_index do |name, at|
        __send__(name, row_box(y, first + at))
        y += row_step
      end
      y
    end

    # Where a row is, and whether it has the focus (`place` is its place in the focus).
    def row_box(y, place)
      { x: row_x, y: y, width: @menu.scaled(ROW_WIDTH), height: @menu.scaled(ROW_HEIGHT),
        focused: focus&.at?(place) || false }
    end

    def row_step = @menu.scaled(ROW_HEIGHT + ROW_GAP)
    def row_x = (UI.width - @menu.scaled(ROW_WIDTH)) / 2

    # A choice among `values`, each shown as `shown` makes it: the one picked, or nil while the
    # current one stays. A current value the list does not have (set by a script) is added to it.
    def pick(label, values, current, box, shown)
      values = (values + [current]).uniq
      index = values.index(current)
      picked = UI.choice(label, values.map(&shown), index, **box)
      values[picked] unless picked == index
    end

    def window_mode(box)
      named = ->(mode) { MODE_NAMES.fetch(mode).call }
      mode = pick(I18n.t("game.settings.window_mode"), Settings::WINDOW_MODES, Settings.window_mode, box, named)
      Settings.window_mode = mode if mode
    end

    def resolution(box)
      sizes = (Settings.resolutions + [Settings.resolution]).uniq.sort_by { |width, height| width * height }
      written = ->((width, height)) { "#{width} x #{height}" }
      size = pick(I18n.t("game.settings.resolution"), sizes, Settings.resolution, box, written)
      Settings.resolution = size if size
    end

    def render_scale(box)
      values = (Settings::RENDER_SCALES + [Settings.render_scale]).uniq.sort
      percent = ->(share) { "#{(share * PERCENT).round}%" }
      scale = pick(I18n.t("game.settings.render_scale"), values, Settings.render_scale, box, percent)
      Settings.render_scale = scale if scale
    end

    def vsync(box)
      on = UI.toggle(I18n.t("game.settings.vsync"), Settings.vsync, **box)
      Settings.vsync = on unless on == Settings.vsync
    end

    def fps_cap(box)
      current = Settings.fps_cap || Settings::NO_CAP
      values = (Settings::FPS_CAPS + [current]).uniq.sort
      written = ->(fps) { fps.zero? ? I18n.t("game.settings.off") : fps.to_s }
      cap = pick(I18n.t("game.settings.fps_cap"), values, current, box, written)
      Settings.fps_cap = cap if cap
    end

    def quality(box)
      named = ->(quality) { QUALITY_NAMES.fetch(quality).call }
      quality = pick(I18n.t("game.settings.quality"), Settings::QUALITIES, Settings.quality, box, named)
      Settings.quality = quality if quality
    end

    def volume(box)
      volume = UI.slider(I18n.t("game.settings.volume"), Settings.volume, step: VOLUME_STEP, **box)
      Settings.volume = volume unless volume == Settings.volume
    end
  end
end
