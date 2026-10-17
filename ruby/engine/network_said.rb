# Network::Said: what arrives from other machines about objects, checked before it is used.
module Network
  # What other machines say about objects, checked before any of it is used: whoever sent it may
  # not be running this game's code. What does not fit is ignored.
  module Said
    MAX_PEERS = 16            # machines in one game, the host included
    RECORD_SIZE = 5           # parts of a spawn record: key, template, owner, position, rotation
    SYNC_SIZE = 3             # parts of what says where an object is: key, position, rotation
    ANIMATION_SIZE = 4        # parts of what says what its animation does: fade, weight, own clip, mixed clip
    CLIP_SIZE = 5             # parts of a clip: name, time, speed, loops, plays
    VECTOR_SIZE = 3           # numbers a position or a rotation travels as: x, y, z
    MAX_KEY_SIZE = 128        # bytes in the key of an object on the network (a spawned one, any NetworkSync)
    MAX_COUNT_DIGITS = 9      # in the number such a key ends with
    MAX_DISTANCE = 1_000_000.0   # meters from the middle of a level (or degrees of a turn) an object can be at

    class << self
      # A spawn record with every part of its kind.
      def record?(record) = list?(record, RECORD_SIZE) && right_parts?(*record)

      # An array of exactly so many things.
      def list?(value, size) = value.is_a?(Array) && value.size == size

      def right_parts?(key, template, owner, position, rotation)
        [key, template].all?(String) && peer_id?(owner) && [position, rotation].compact.all? { |part| vector?(part) }
      end

      def peer_id?(owner) = owner.is_a?(Integer) && owner.between?(HOST, MAX_PEERS)

      # Three numbers an object can be at, or be turned by.
      def vector?(part) = list?(part, VECTOR_SIZE) && part.all? { |number| reachable?(number) }

      # (Infinity and what is not a number compare as false, so they are out too.)
      def reachable?(number) = number.is_a?(Numeric) && number.abs <= MAX_DISTANCE

      # Where an object is, as its owner sends it: [key, position, rotation], and what its animation
      # does after them when that came along.
      def placement?(data)
        key, position, rotation, animation = data
        sync_size?(data) && key.is_a?(String) && vector?(position) && vector?(rotation) && no_or_animation?(animation)
      end

      def sync_size?(data) = list?(data, SYNC_SIZE) || list?(data, SYNC_SIZE + 1)
      def no_or_animation?(part) = part.nil? || animation?(part)

      # [fade, weight, own clip, mixed clip]: numbers, and clips or nils.
      def animation?(part)
        list?(part, ANIMATION_SIZE) && numbers?(part.take(2)) && part.drop(2).all? { |clip| no_or_clip?(clip) }
      end

      def no_or_clip?(clip) = clip.nil? || clip?(clip)
      # [name, time, speed, loops, plays].
      def clip?(clip) = list?(clip, CLIP_SIZE) && clip[0].is_a?(String) && numbers?(clip[1, 2])
      def numbers?(list) = list.all? { |number| reachable?(number) }

      # Whether `key` is one `maker` gives what it spawns from `template`: "Ball#2.7" is the
      # seventh thing machine 2 spawned. Nothing else is a spawned object's key, so no machine
      # can take a key another one's objects go by, now or later.
      def key_of?(key, template, maker)
        start = "#{template}##{maker}."
        key.size <= MAX_KEY_SIZE && key.start_with?(start) && count?(key[start.size, key.size])
      end

      # A count as it is written into a key: digits, with no sign and no zero in front.
      def count?(text) = text.size.between?(1, MAX_COUNT_DIGITS) && text.to_i.positive? && text.to_i.to_s == text
    end
  end
end
