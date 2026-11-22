# Hit boxes on an animated character: a volume under each bone that matters (head, chest, arms,
# legs), kept where the bone is in every frame the character is posed, so a shot hits the arm that
# is there now. A ray that meets one says which part of the body it hit: Hitboxes.region(hit).
# They are posed wherever the game runs, a dedicated server too, so the host can judge the hits.
#
#   Hitboxes.build(soldier, {
#     "Head"     => { region: :head, size: 0.25, offset: [0, 0.1, 0] },
#     "Spine2"   => { region: :chest, size: [0.4, 0.35, 0.25] },
#     "LeftArm"  => { region: :arm, shape: :capsule, size: [0.12, 0.3, 0.12], offset: [0, 0.15, 0] },
#   }, layer: "hitbox")
#
#   hit = Physics.raycast(eye, aim, 100, layers: ["hitbox"])
#   damage = { head: 100, chest: 30, arm: 15 }.fetch(Hitboxes.region(hit), 0) if hit
module Hitboxes
  DEFAULT_SIZE = 0.2  # meters a side
  SHAPES = %i[box capsule sphere].freeze

  # Makes a hit box under each bone of `character` the map names ("prefix:Name" bones answer to
  # "Name"): region (its name, the bone's name unless said), size (meters, one number or
  # [width, height, depth]; a capsule stands along the bone's up), offset (meters from the bone, in
  # its own axes) and shape (:box, :capsule or :sphere). layer: the collision layer of every box
  # (the default layer unless said; one game.json lacks is refused). Returns the boxes made, in the
  # map's order. Build them once for a character: each call adds boxes, and a copy spawned of it
  # carries its own.
  def self.build(character, map, layer: nil)
    animator = character.animation
    map.map { |bone, settings| __box(animator.bone!(bone), bone.to_s, layer.to_s, **settings) }
  end

  def self.__box(bone, name, layer, region: name, size: DEFAULT_SIZE, offset: Vector3.zero, shape: :box)
    unless SHAPES.include?(shape)
      raise ArgumentError, "Hitboxes.build: a shape is one of #{SHAPES.inspect}, not #{shape.inspect}"
    end

    __make(bone, "#{name} hitbox", shape.to_s, __size(size), __vector(offset), layer, region.to_s)
  end

  def self.__size(size) = size.is_a?(Numeric) ? Vector3.new(size, size, size) : __vector(size)

  def self.__vector(value) = value.is_a?(Array) ? Vector3.new(*value) : value

  # The part of the body a raycast hit (Physics.raycast's Hash) met, as a Symbol: nil for no hit, or
  # a hit on anything but a hit box.
  def self.region(hit)
    props = __hitbox_props(hit)
    props[:region]&.to_sym if props
  end

  # The props of the hit box a hit met, or nil for no hit, or one on anything else.
  def self.__hitbox_props(hit)
    props = hit && hit[:object]&.props
    props if props&.[](:hitbox)
  end
end
