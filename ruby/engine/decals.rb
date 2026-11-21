# Marks laid on the scene's surfaces: bullet holes, scorches, cracks, footprints. A decal is a
# picture projected onto whatever is under it (walls, floors, terrain, props), and it stays on it:
# a door that swings carries its marks along. Decals darken what they lie on, so they take its
# light and shade. They are looks only, kept on this machine, up to Decals.limit (the oldest goes).
#
#   hit = Physics.raycast(eye, aim, 100)
#   Decals.place(hit) if hit
#   Decals.place(hit[:point], hit[:normal], image: :bullet_hole, size: 0.08, spin: rand(360))
#   Decals.place(hit, color: :maroon, alpha: 200, size: 0.5)
module Decals
  DEFAULT_SIZE = 0.15  # meters across
  NO_SPIN = 0.0

  # A decal at `point` on a surface facing `normal` (or at a raycast hit: its point and normal).
  # size: meters across; depth: how far into and out of the surface it reaches (as much as its size
  # unless said: a mark across a corner falls on both sides). image: a picture of the game's images/
  # folder (image: :bullet_hole draws images/bullet_hole.png), dark where it marks most and see-through
  # where it marks nothing; without one it is a soft round spot. color tints it, and alpha (0 to 255)
  # says how much it darkens. spin: degrees it turns about the normal. Returns whether it fell on
  # anything.
  def self.place(point, normal = nil, size: DEFAULT_SIZE, depth: nil, image: nil, color: :black,
                 alpha: UI::OPAQUE, spin: NO_SPIN)
    point, normal = __at(point, normal)
    red, green, blue = UI.rgb(color)
    __place(point, normal, size.to_f, (depth || size).to_f, spin.to_f, image.to_s, red.to_f, green.to_f,
            blue.to_f, alpha.to_f)
  end

  # A point and a normal, or a raycast hit ({point:, normal:, ...}) for both.
  def self.__at(point, normal)
    return [point, normal] unless point.is_a?(Hash)

    [point.fetch(:point), point.fetch(:normal)]
  end
end
