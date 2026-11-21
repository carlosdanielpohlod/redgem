# How the interface looks: the fonts, sizes, colors and shapes UI's widgets and GameMenu's screens
# are drawn with, and how a menu shows itself (its background, logo, footer, transitions, sounds and
# music). A theme is the defaults (DEFAULTS: the engine's own look) with layers of changes over
# them, each a Hash of only what it changes:
#
#   UI.theme = { colors: { button: "#203040", hover: :gold }, button: { radius: 0 } }   # every widget
#   GameMenu.theme = { font: "Bangers", layout: { align: :left } }                     # the menus
#   menu.screens[:title].theme = { background: { kind: :gradient, colors: [:ruby, :black] } }
#
# game.json says the same for the menus, its keys as the names here: "menu": {"theme": {...},
# "screens": {"title": {...}}}. Colors are a name of UI::COLORS, "#rrggbb" or [r, g, b]; fonts are
# faces of fonts/ (UI.font), images pictures of images/, sounds files of sounds/. Every size is in
# the units the menu is laid out in (a 720 unit high screen, see GameMenu#scale). A key a theme does
# not have, or a value it cannot take, is refused with what is wrong (`redgem check` says it of
# game.json).
module UI
  class Theme
    ALIGNS = %i[left center right].freeze
    HEADINGS = %i[above top none].freeze      # over the buttons, at the top of the screen, or none
    BACKGROUNDS = %i[scene color gradient image].freeze
    FITS = %i[cover contain stretch].freeze
    TRANSITIONS = %i[none fade slide].freeze
    ANCHOR_NAMES = %i[top_left top top_right left center right bottom_left bottom bottom_right].freeze
    # What each key holds: a section (a Hash of keys), the kind of its value, or the names it may
    # be. A kind ending in "?" may also be nil (none: no shade, no logo...).
    SCHEMA = {
      font: :font?, heading_font: :font?,
      sizes: { heading: :size, caption: :size, text: :size, note: :size, footer: :size, label_share: :share },
      colors: { text: :color, heading: :color, caption: :color, muted: :color, label: :color, button: :color,
                button_alpha: :alpha, hover: :color, focused: :color, pressed: :color?, pressed_shade: :share,
                disabled: :color, disabled_text: :color, border: :color, focus_ring: :color, accent: :color,
                track: :color, track_alpha: :alpha, scrollbar_alpha: :alpha, handle_alpha: :alpha, cell: :color,
                cell_alpha: :alpha, conflict: :color, done: :color, to_do: :color, shade: :color?,
                shade_alpha: :alpha },
      button: { width: :size, max_width: :size?, height: :size, gap: :count, radius: :count, border: :count,
                padding: :count, focus_ring: :count, skin: :image?, skin_focused: :image?, skin_pressed: :image?,
                skin_disabled: :image?, skin_edge: :count, icons: :icons, icon_share: :share },
      layout: { align: ALIGNS, top: :share, margins: :margins, heading: HEADINGS, heading_gap: :number,
                heading_top: :number },
      background: { kind: BACKGROUNDS, color: :color, colors: :colors, image: :image?, fit: FITS, blur: :share,
                    camera_path: { points: :points, look_at: :point?, seconds: :size } },
      logo: { image: :image?, anchor: ANCHOR_NAMES, x: :number, y: :number, scale: :size, width: :size? },
      footer: { text: :text?, anchor: ANCHOR_NAMES, x: :number, y: :number, color: :color },
      transition: { kind: TRANSITIONS, seconds: :count },
      sounds: { move: :sound?, pick: :sound?, back: :sound?, volume: :share, group: :group },
      music: { name: :sound?, volume: :share, fade: :count }
    }.freeze
    # The engine's own look: what a theme that changes nothing draws.
    DEFAULTS = {
      font: nil, heading_font: nil,
      sizes: { heading: 72, caption: 20, text: 20, note: 20, footer: 16, label_share: 0.42 },
      colors: { text: :white, heading: :white, caption: :gold, muted: :gray, label: :white, button: :dark,
                button_alpha: 225, hover: :ruby, focused: :ruby, pressed: nil, pressed_shade: 0.8, disabled: :dark,
                disabled_text: :gray, border: :white, focus_ring: :gold, accent: :ruby, track: :gray,
                track_alpha: 255, scrollbar_alpha: 70, handle_alpha: 200, cell: :black, cell_alpha: 140,
                conflict: :orange, done: :green, to_do: :gray, shade: :black, shade_alpha: 170 },
      button: { width: 280, max_width: nil, height: 58, gap: 16, radius: 10, border: 0, padding: 14, focus_ring: 3,
                skin: nil, skin_focused: nil, skin_pressed: nil, skin_disabled: nil, skin_edge: 16, icons: {},
                icon_share: 0.6 },
      layout: { align: :center, top: 0.46, margins: [0, 0, 0, 0], heading: :above, heading_gap: 130,
                heading_top: 40 },
      background: { kind: :scene, color: :black, colors: %i[dark black], image: nil, fit: :cover, blur: 0.0,
                    camera_path: { points: [], look_at: nil, seconds: 20 } },
      logo: { image: nil, anchor: :top, x: 0, y: 40, scale: 1.0, width: nil },
      footer: { text: nil, anchor: :bottom_right, x: -16, y: -12, color: :gray },
      transition: { kind: :none, seconds: 0.25 },
      sounds: { move: nil, pick: nil, back: nil, volume: 1.0, group: :effects },
      music: { name: nil, volume: 0.7, fade: 1.0 }
    }.freeze
    # The keys of game.json's "menu" section: the menus' theme, and each screen's own over it.
    MENU_KEYS = %w[theme screens].freeze
    NOT_MENU_KEY = "is not a menu setting (it has: theme, screens)".freeze
    # The folders of the game the kinds of value that name a file are in.
    FOLDERS = { font: "fonts", image: "images", sound: "sounds" }.freeze

    # Bumped whenever a layer changes, so that whoever keeps a theme made of layers makes it again.
    def self.version = @version ||= 0
    def self.changed = @version = version + 1

    def self.default = @default ||= new(DEFAULTS)

    # A layer as a theme takes it (its keys as symbols, its values as the theme draws them), from a
    # script's Hash or game.json's; raises an ArgumentError that says what is wrong with it.
    def self.read!(layer)
      read, problems = read(layer)
      raise ArgumentError, "theme: #{problems.join("; ")}" unless problems.empty?

      read
    end

    # A layer read: [what can be used of it, what is wrong with the rest (a line each)].
    def self.read(layer, where = "theme")
      reading = Reading.new
      [reading.section(layer || {}, SCHEMA, [where]), reading.problems]
    end

    # `base` with `layer` over it: a section of both is merged key by key, anything else replaced.
    def self.merge(base, layer)
      merged = base.dup
      layer.each { |key, value| merged[key] = both_sections?(merged[key], value) ? merge(merged[key], value) : value }
      merged
    end

    def self.both_sections?(one, other) = one.is_a?(Hash) && other.is_a?(Hash)

    # game.json's "menu" section, read: [{theme: layer, screens: {name => layer}}, problems].
    def self.read_menu(settings)
      menu = MenuReading.new(settings)
      [menu.read, menu.problems]
    end

    # What `redgem check` asks of game.json's "menu" section: [problems, [[folder, name], ...]] for
    # every file it names, which the check looks for.
    def self.__check_settings
      menu, problems = read_menu(__menu_settings)
      layers = [menu[:theme]] + menu[:screens].values
      [problems, layers.flat_map { |layer| files_of(layer, SCHEMA) }.uniq]
    end

    def self.__menu_settings = UI.__menu_settings || {}

    # The files a layer names: [folder, name] each.
    def self.files_of(layer, schema)
      layer.flat_map do |key, value|
        kind = schema[key]
        kind.is_a?(Hash) ? files_of(value, kind) : Kinds.files(kind, value)
      end
    end

    def initialize(values)
      @values = values
    end

    # What a key holds, by its path: theme[:colors, :button].
    def [](*path) = path.reduce(@values) { |at, key| at.fetch(key) }

    # A color of the colors section, as [r, g, b].
    def color(name) = UI.rgb(self[:colors, name])

    # This theme with a layer (already read: see Theme.read!) over it.
    def merged(layer) = layer.empty? ? self : Theme.new(Theme.merge(@values, layer))

    def to_h = @values

    # Goes through a layer key by key against the schema, keeping what it can use and saying what
    # is wrong with the rest.
    class Reading
      attr_reader :problems

      def initialize
        @problems = []
      end

      def section(given, schema, path)
        return problem(path, "must be a table of settings (a Hash)", {}) unless given.is_a?(Hash)

        read = {}
        given.each { |key, value| entry(read, key, value, schema, path) }
        read
      end

      private

      # (A key is matched against the names the schema has, never made a symbol of its own.)
      def entry(read, key, value, schema, path)
        name = schema.keys.find { |known| known.to_s == key.to_s }
        return unknown(path + [key], schema) unless name

        kind = schema[name]
        return read[name] = section(value, kind, path + [name]) if kind.is_a?(Hash)

        leaf(read, name, value, kind, path + [name])
      end

      def leaf(read, name, value, kind, path)
        taken = Kinds.read(kind, value)
        return problem(path, "must be #{Kinds.wanted(kind)}, not #{value.inspect}", nil) if Kinds.bad?(taken)

        read[name] = taken
      end

      def unknown(path, schema)
        problem(path, "is not a theme setting (#{path[-2]} has: #{schema.keys.join(", ")})", nil)
      end

      def problem(path, text, result)
        @problems << "#{path.join(".")} #{text}"
        result
      end
    end

    # Reads game.json's "menu" section: its theme, and its screens' own.
    class MenuReading
      attr_reader :problems

      def initialize(settings)
        @settings = settings
        @problems = []
      end

      def read
        return refused({}) unless @settings.is_a?(Hash)

        @settings.each_key { |key| @problems << "#{key} #{NOT_MENU_KEY}" unless known?(key) }
        { theme: layer(@settings["theme"], "theme"), screens: screens(@settings["screens"]) }
      end

      private

      def known?(key) = MENU_KEYS.include?(key.to_s)

      def refused(result)
        @problems << "must be a table of settings (a Hash)"
        { theme: result, screens: {} }
      end

      def layer(given, where)
        read, problems = Theme.read(given, where)
        @problems.concat(problems)
        read
      end

      def screens(given)
        return {} if given.nil?
        return refused({})[:screens] unless given.is_a?(Hash)

        read = {}
        given.each { |name, value| read[name.to_s] = layer(value, "screens.#{name}") }
        read
      end
    end
  end
end
