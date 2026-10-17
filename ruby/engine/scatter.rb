# Copies of an object's mesh in great numbers that are not objects of their own: a meadow's grass,
# a forest's trees, the rubble of a ruined city. They are only drawn (nothing collides with them or
# runs on them), so a scene can have tens of thousands; and they are drawn a few at a time, by
# patches of ground, which is far cheaper than as many objects. The template is usually a prefab,
# hidden in the game; the copies take its mesh (not its children's), tint and drawing distances
# as they are when placed. Where they go depends only on the seed: every machine of a game, the
# dedicated server's included, places the same copies.
#
#   meadow = Scatter.place("GrassTuft", count: 50_000, area: [Vector3[-80, 0, -80], Vector3[80, 0, 80]],
#                          ground: Scene.find("Ground"), seed: 7, scale: 0.7..1.4, align: true)
#   meadow.count    # how many found ground to stand on
#   meadow.remove
class Scatter
  attr_reader :count

  # area: two opposite corners of a box. ground: an object with a collider: each copy stands where a
  # ray straight down meets it first, and a try that meets something else first, or nothing,
  # places none (the corners' heights then hardly matter); without it, copies stand on the box's
  # bottom. scale: a number, or a range each copy is scaled within. align: true leans them with
  # the slope they stand on (grass); false keeps them upright (trees, buildings).
  def self.place(template, count:, area:, ground: nil, seed: 0, scale: 1.0, align: false)
    low, high = area
    smallest, largest = scale.is_a?(Range) ? [scale.begin, scale.end] : [scale, scale]
    id, placed = __place(template, count, low, high, ground, seed, smallest.to_f, largest.to_f, align)
    new(id, placed)
  end

  def initialize(id, count)
    @id = id
    @count = count
  end

  # Takes the copies away: true the first time.
  def remove = Scatter.__remove(@id)
end
