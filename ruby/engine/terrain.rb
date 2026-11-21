# The ground layers of a terrain (experimental): which one is where, for footsteps or what grows
# there, and the weights painted again by rules while the game runs. A terrain has ground layers
# when its object names a layers file (terrain_layers = "island", see terrains/island.json).
#
#   ground = Scene.find("Island")
#   Terrain.layer_at(ground, player.position)      # => "sand"
#   Terrain.weights_at(ground, player.position)    # => {"sand" => 0.7, "grass" => 0.3}
#   Terrain.paint_by_rules(ground)                 # the rules of its layers file, again
#   Terrain.paint_by_rules(ground, [{layer: "rock", slope: {min: 30, soft: 8}}])
module Terrain
  # The names of the object's layers, in order; nil when it has none.
  def self.layers(object) = __layers(object)

  # Each layer's share of the ground under a point, seen from above ({name => 0 to 1}); nil
  # without layers, or past the terrain's edges.
  def self.weights_at(object, position)
    names = __layers(object)
    weights = names && __weights_at(object, position)
    weights && names.zip(weights).to_h
  end

  # The layer with the largest share under a point; nil without layers, or past the edges.
  def self.layer_at(object, position) = weights_at(object, position)&.max_by { |_name, share| share }&.first

  # Paints the weights by rules (an Array of Hashes, as the layers file's "rules"; nil: the file's
  # own). Returns how many texels changed. Kept while the scene runs, never written to the files.
  def self.paint_by_rules(object, rules = nil) = __paint(object, rules)
end
