# The kinds of value a theme's settings take (see UI::Theme::SCHEMA): what each accepts, what it
# keeps of it (a color's name as a symbol, "#ff8800" as [255, 136, 0], a point as a Vector3), and
# what to say when a value is not one of them.
module UI
  class Theme
    module Kinds
      BAD = Object.new.freeze  # what reading a value a kind does not take gives
      HEX_MARK = "#".freeze
      HEX_LETTERS = "0123456789abcdef".freeze
      HEX_DIGITS = 2           # of each channel of "#rrggbb"
      HEX_BASE = 16
      CHANNELS = 3
      SIDES = 4                # of margins: top, right, bottom, left
      GRADIENT_COLORS = 2      # its top's and its bottom's
      POINT_PARTS = 3
      MOST_FACES = 4           # a font's fallback chain (UI.font=)
      LARGEST_CHANNEL = 255
      NAMED_FILES = %i[font image sound].freeze
      # What each kind is said to be when a value is not one.
      WANTED = {
        font: "a font of fonts/ (a name, or a list of up to 4)", size: "a number above 0", count: "a number, 0 or more",
        number: "a number", share: "a number from 0 to 1", alpha: "a whole number from 0 to 255",
        color: "a color (a name such as \"gold\", \"#rrggbb\" or [r, g, b])", colors: "a list of two colors",
        image: "a picture of images/ (its name)", sound: "a sound of sounds/ (its name)", text: "a text",
        group: "an audio group's name (such as \"effects\")", icons: "a table of option names and pictures",
        margins: "a number, 0 or more, or [top, right, bottom, left]", points: "a list of points ([x, y, z] each)",
        point: "a point ([x, y, z])"
      }.freeze

      # The value as a theme keeps it, or BAD.
      def self.read(kind, value)
        return enum(kind, value) if kind.is_a?(Array)
        return nil if value.nil? && optional?(kind)

        __send__("read_#{base(kind)}", value)
      end

      def self.bad?(value) = value.equal?(BAD)

      def self.wanted(kind)
        return "one of #{kind.join(", ")}" if kind.is_a?(Array)

        text = WANTED.fetch(base(kind))
        optional?(kind) ? "#{text}, or nothing (null)" : text
      end

      # The files a value of this kind names: [folder, name] each.
      def self.files(kind, value)
        return [] if names_no_file?(kind, value)
        return value.values.map { |name| [FOLDERS[:image], name] } if base(kind) == :icons

        Array(value).map { |name| [FOLDERS.fetch(base(kind)), name] }
      end

      # Whether a value names no file: none given, or of a kind that is no file's.
      def self.names_no_file?(kind, value)
        value.nil? || kind.is_a?(Array) || !(NAMED_FILES + [:icons]).include?(base(kind))
      end

      def self.optional?(kind) = kind.to_s.end_with?("?")
      def self.base(kind) = kind.to_s.chomp("?").to_sym
      def self.named?(value) = (value.is_a?(String) || value.is_a?(Symbol)) && !value.to_s.empty?
      def self.number?(value) = value.is_a?(Numeric)

      def self.enum(names, value)
        names.find { |name| name.to_s == value.to_s } || BAD
      end

      def self.read_font(value)
        return value.to_s if named?(value)

        faces?(value) ? value.map(&:to_s) : BAD
      end

      def self.faces?(value) = list?(value, 1..MOST_FACES) && value.all? { |face| named?(face) }

      # Whether a value is a list of as many things as `sizes` allows.
      def self.list?(value, sizes) = value.is_a?(Array) && sizes.include?(value.size)

      def self.read_size(value) = number?(value) && value.positive? ? value : BAD
      def self.read_count(value) = number?(value) && !value.negative? ? value : BAD
      def self.read_number(value) = number?(value) ? value : BAD
      def self.read_share(value) = number?(value) && value.between?(0, 1) ? value : BAD
      def self.read_alpha(value) = number?(value) && value.between?(0, LARGEST_CHANNEL) ? value.to_i : BAD
      def self.read_text(value) = value.is_a?(String) ? value : BAD
      def self.read_image(value) = named?(value) ? value.to_s : BAD
      def self.read_sound(value) = named?(value) ? value.to_s : BAD
      def self.read_group(value) = named?(value) ? value.to_s.to_sym : BAD

      def self.read_color(value)
        return hex(value) if hex?(value)

        channels?(value) ? value.map(&:to_i) : color_named(value)
      end

      # A color of UI::COLORS by its name (never a symbol made of a name it does not have).
      def self.color_named(value)
        return BAD unless named?(value)

        UI::COLORS.keys.find { |name| name.to_s == value.to_s } || BAD
      end

      def self.channels?(value)
        list?(value, [CHANNELS]) && value.all? { |part| number?(part) && part.between?(0, LARGEST_CHANNEL) }
      end

      # Whether a value is written "#rrggbb" (mruby has no regular expressions to say it with).
      def self.hex?(value)
        written = value.is_a?(String) && value.size == HEX_MARK.size + CHANNELS * HEX_DIGITS
        written && value.start_with?(HEX_MARK) && hex_digits?(value[HEX_MARK.size..])
      end

      def self.hex_digits?(text) = text.downcase.each_char.all? { |letter| HEX_LETTERS.include?(letter) }

      # "#ff8800" -> [255, 136, 0].
      def self.hex(text) = (0...CHANNELS).map { |at| text[HEX_MARK.size + at * HEX_DIGITS, HEX_DIGITS].to_i(HEX_BASE) }

      def self.read_colors(value)
        list?(value, [GRADIENT_COLORS]) ? each_read(value) { |color| read_color(color) } : BAD
      end

      # What reading each thing of a list gives, or BAD when one is.
      def self.each_read(list, &)
        read = list.map(&)
        read.any? { |one| bad?(one) } ? BAD : read
      end

      def self.read_margins(value)
        return [value] * SIDES if number?(value) && !value.negative?

        sides?(value) ? value : BAD
      end

      def self.sides?(value) = list?(value, [SIDES]) && value.all? { |side| number?(side) && !side.negative? }

      def self.read_point(value)
        return value if value.is_a?(Vector3)

        parts?(value) ? Vector3.new(*value) : BAD
      end

      def self.parts?(value) = list?(value, [POINT_PARTS]) && value.all? { |part| number?(part) }

      def self.read_points(value) = value.is_a?(Array) ? each_read(value) { |point| read_point(point) } : BAD

      def self.read_icons(value)
        return BAD unless value.is_a?(Hash) && value.values.all? { |name| named?(name) }

        icons = {}
        value.each { |option, name| icons[option.to_s] = name.to_s }
        icons
      end
    end
  end
end
