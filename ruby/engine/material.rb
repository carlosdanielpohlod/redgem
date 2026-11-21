# The glow of the scene's materials, by the name they have in the scene's file, changed while the
# game runs: lit windows at night, a sign switched on. Every material of that name glows, in every
# object that has it, spawned copies and later scenes too, until reset_emission.
#
#   Material.set_emission("window_glass", [255, 190, 120], strength: 4.0)
#   Material.reset_emission("window_glass")    # as the file says again
module Material
  CHANNEL_MAX = 255.0 # a color's channels go from 0 to this
  RGB = 3

  # color: a name (:orange) or [r, g, b] from 0 to 255; strength: how bright, 1 being the color as it is.
  def self.set_emission(name, color, strength: 1.0)
    light = UI.rgb(color).first(RGB).map { |channel| channel / CHANNEL_MAX * strength }
    __set_emission(name.to_s, *light)
    __emissions[name.to_s] = { color: color, strength: strength }
    nil
  end

  # What set_emission gave the materials of that name ({color:, strength:}), or nil.
  def self.emission(name) = __emissions[name.to_s]

  def self.__emissions = (@__emissions ||= {})

  def self.reset_emission(name)
    __emissions.delete(name.to_s)
    __reset_emission(name.to_s)
  end
end
