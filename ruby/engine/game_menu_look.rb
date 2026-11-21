# What a menu shows around its screens, as their theme says (see UI::Theme): the background (the
# game frozen behind a shade, a color, a gradient or a picture), a logo and a footer; the scene
# behind it blurred, or seen from a camera that flies a path; the screens coming in (a fade or a
# slide); and its sounds and music, heard through the audio groups (so the player's volumes apply).
class GameMenu < Behaviour
  class Look
    SLIDE_SHARE = 0.25   # of the screen's width a screen slides in from
    EASING = 2           # the transition slows down as the square of what is left of it
    NO_BLUR = 0.0

    def initialize(menu)
      @menu = menu
      @coming = 0.0          # seconds of the transition still to go
      @music = nil           # the menu's track playing, [name, fade out seconds]
      @path = nil            # the camera's path while one flies
    end

    # A screen was shown, drawn with `theme`.
    def shown(theme)
      @transition = theme[:transition]
      @coming = @transition[:kind] == :none ? 0.0 : @transition[:seconds].to_f
      play_music(theme[:music])
      scene_behind(theme[:background])
    end

    # The menu closed: its music ends, and the camera and the picture are the game's again.
    def left
      stop_music
      blur(NO_BLUR) unless @blur_was.nil?
      @blur_was = nil
      land
    end

    # The background (or what `custom` draws instead), the logo and the footer.
    def backdrop(custom)
      theme = UI.theme
      custom ? custom.call : background(theme)
      logo(theme[:logo])
      footer(theme[:footer], theme[:sizes, :footer])
    end

    # Draws what the block draws as the transition has it now: coming in from the right, or fading in.
    def coming_in(&)
      share = transition_share
      @coming = [@coming - Time.unscaled_delta, 0.0].max
      return yield if share >= 1.0

      @transition[:kind] == :slide ? UI.shifted(x: slide(share), &) : UI.shifted(alpha: share, &)
    end

    # Plays one of the theme's sounds (:move, :pick or :back), when it has that one.
    def sound(theme, kind)
      sounds = theme[:sounds]
      Audio.play(sounds[kind], volume: sounds[:volume], group: sounds[:group]) if sounds[kind]
    end

    private

    # How far the screen has come in, 0 to 1 (eased: fast, then slower).
    def transition_share
      return 1.0 unless @coming.positive?

      done = 1.0 - @coming / @transition[:seconds]
      1.0 - (1.0 - done)**EASING
    end

    def slide(share) = ((1.0 - share) * UI.width * SLIDE_SHARE).to_i

    def full = { x: 0, y: 0, width: UI.width, height: UI.height }

    def background(theme)
      background = theme[:background]
      case background[:kind]
      when :color then UI.rect(**full, color: background[:color])
      when :gradient then UI.gradient(**full, top: background[:colors][0], bottom: background[:colors][1])
      when :image then picture(background)
      else shade(theme[:colors])
      end
    end

    # The game behind the menu, darkened (unless the theme has no shade).
    def shade(colors)
      UI.rect(**full, color: colors[:shade], alpha: colors[:shade_alpha]) if colors[:shade]
    end

    # A picture over the screen: covering it (cut at two sides), fit inside it (on its color), or
    # stretched over it.
    def picture(background)
      UI.rect(**full, color: background[:color])
      name = background[:image]
      UI.image(name, **fitted(UI.image_size(name), background[:fit])) if name
    end

    def fitted(size, fit)
      return full if size.nil? || fit == :stretch

      shares = [UI.width.to_f / size[0], UI.height.to_f / size[1]]
      grown = fit == :cover ? shares.max : shares.min
      middled((size[0] * grown).to_i, (size[1] * grown).to_i)
    end

    # A box that size in the middle of the screen.
    def middled(width, height)
      { x: (UI.width - width) / 2, y: (UI.height - height) / 2, width: width, height: height }
    end

    def logo(logo)
      name = logo[:image]
      return unless name

      width, height = logo_size(name, logo)
      UI.image(name, anchor: logo[:anchor], x: @menu.scaled(logo[:x]), y: @menu.scaled(logo[:y]), width: width,
                     height: height)
    end

    # As wide as the theme says, or as its picture is times its scale; as tall as its shape makes it.
    def logo_size(name, logo)
      size = UI.image_size(name)
      width = logo_width(size, logo)
      size && width.positive? ? [width, width * size[1] / size[0]] : [width, 0]
    end

    # (0 for a picture of no known size, given no width: drawn at its own then.)
    def logo_width(size, logo)
      return @menu.scaled(logo[:width]) if logo[:width]

      size ? @menu.scaled(size[0] * logo[:scale]) : 0
    end

    # The footer's text, its {version} and {title} filled in (a key of the game's texts is looked up).
    def footer(footer, size)
      text = footer[:text]
      return unless text

      UI.text(footer_text(text), anchor: footer[:anchor], x: @menu.scaled(footer[:x]), y: @menu.scaled(footer[:y]),
                                 size: @menu.scaled(size), color: footer[:color])
    end

    def footer_text(text)
      values = { version: Game.version.to_s, title: Game.title.to_s }
      shown = I18n.t(text, **values)
      values.reduce(shown) { |filled, (name, value)| filled.gsub("{#{name}}", value) }
    end

    def play_music(music)
      name = music[:name]
      return if name.nil? || @music&.first == name

      @music = [name, music[:fade]] if Audio.music(name, volume: music[:volume], fade: music[:fade])
    end

    def stop_music
      return unless @music

      Audio.stop_music(fade: @music.last)
      @music = nil
    end

    # The scene behind a screen: blurred as its theme says, seen from a camera on a path when it has one.
    def scene_behind(background)
      scene = background[:kind] == :scene
      blur(scene ? background[:blur] : NO_BLUR)
      path = background[:camera_path]
      scene && !path[:points].empty? ? fly(path) : land
    end

    # (What the game had is kept, to give back.)
    def blur(amount)
      return if amount.zero? && @blur_was.nil?

      @blur_was = Camera.backdrop_blur if @blur_was.nil?
      Camera.backdrop_blur = amount.zero? ? @blur_was : amount
    end

    def fly(path)
      return if @path&.flies?(path) || Camera.main.nil?

      land
      rig = Camera.__rig("GameMenu's camera path")
      @path = CameraPath.new(path, rig.motion, Camera.main.transform)
      rig.motion = @path
    end

    def land
      return unless @path

      @path.give_back(Camera.__current_rig, Camera.main&.transform)
      @path = nil
    end
  end
end
