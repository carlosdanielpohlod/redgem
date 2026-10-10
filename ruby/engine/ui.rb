# Immediate-mode HUD drawing. Call it every frame from update.
module UI
  DEFAULT_SIZE = 24
  OPAQUE = 255

  COLORS = {
    white: [255, 255, 255],
    black: [0, 0, 0],
    red: [230, 41, 55],
    green: [0, 228, 48],
    blue: [0, 121, 241],
    yellow: [253, 249, 0],
    gold: [255, 203, 0],
    orange: [255, 161, 0],
    gray: [130, 130, 130]
  }.freeze

  def self.text(text, x:, y:, size: DEFAULT_SIZE, color: :white, alpha: OPAQUE)
    rgb = color.is_a?(Array) ? color : COLORS.fetch(color) { raise ArgumentError, "unknown color #{color.inspect}" }
    __text(text.to_s, x.to_i, y.to_i, size.to_i, rgb[0], rgb[1], rgb[2], alpha.to_i)
  end
end
