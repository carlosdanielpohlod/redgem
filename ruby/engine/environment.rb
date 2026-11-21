# The scene's look while the game runs: its sky, fog, light and effects, as the environment object
# (environment = true) describes them. A change shows on the next frame drawn. Colors are names
# (:orange), [r, g, b] from 0 to 255, or "#rrggbb"; nil gives a setting back to its default.
#
#   Environment[:sky_intensity] = 0.2
#   Environment.set(fog_color: [40, 50, 70], fog_density: 0.004, sky_image: "night_sky")
#
# Each machine keeps its own: over the network, change it on every machine.
module Environment
  COLORS = %i[sky_top sky_horizon ground_color fog_color].freeze
  HEX = "#%02x%02x%02x".freeze
  CHANNELS = 0..255 # each of a color's red, green and blue
  RGB = 3

  def self.[](key) = __get(key.to_s)

  def self.[]=(key, value)
    __set(key.to_s, COLORS.include?(key.to_sym) ? __hex(value) : value)
  end

  def self.set(**settings)
    settings.each { |key, value| self[key] = value }
    nil
  end

  # A color as the environment object keeps it: "#rrggbb" (nil stays nil).
  def self.__hex(color)
    return color if color.nil? || color.is_a?(String)

    format(HEX, *UI.rgb(color).first(RGB).map { |channel| channel.round.clamp(CHANNELS.min, CHANNELS.max) })
  end
end
