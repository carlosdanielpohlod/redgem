# The scene's look while the game runs: its sky, fog, light and effects, as the environment object
# (environment = true) describes them. A change shows on the next frame drawn. Colors are names
# (:orange), [r, g, b] from 0 to 255, or "#rrggbb"; nil gives a setting back to its default.
#
#   Environment[:sky_intensity] = 0.2
#   Environment.set(fog_color: [40, 50, 70], fog_density: 0.004, sky_image: "night_sky")
#
# Each machine keeps its own: over the network, change it on every machine.
module Environment
  COLORS = %w[sky_top sky_horizon ground_color fog_color].freeze
  HEX = "#%02x%02x%02x".freeze
  CHANNELS = 0..255 # each of a color's red, green and blue
  RGB = 3

  def self.[](key) = __get(key.to_s)

  def self.[]=(key, value)
    __set(key.to_s, COLORS.include?(key.to_s) ? __hex(value, key) : value)
  end

  # Changes several settings: all of them, or none when one is refused.
  def self.set(**settings)
    kept = settings.keys.to_h { |key| [key, self[key]] }
    settings.each { |key, value| self[key] = value }
    nil
  rescue StandardError
    __restore(kept)
    raise
  end

  # Gives back what the settings were before a set that was refused (nothing when none was read).
  def self.__restore(kept) = kept&.each { |key, value| __set(key.to_s, value) }

  # A color as the environment object keeps it: "#rrggbb" (nil and Strings as they are).
  def self.__hex(color, key)
    return color if color.nil? || color.is_a?(String)

    channels = UI.rgb(color)
    refused = "Environment #{key} takes [r, g, b] from 0 to 255, not #{color.inspect}"
    raise ArgumentError, refused unless __rgb?(channels)

    format(HEX, *channels.first(RGB).map(&:round))
  end

  def self.__rgb?(channels)
    channels.is_a?(Array) && channels.size >= RGB && channels.first(RGB).all? { |channel| __channel?(channel) }
  end

  def self.__channel?(channel) = channel.is_a?(Numeric) && channel.to_f.finite? && CHANNELS.cover?(channel)
end
