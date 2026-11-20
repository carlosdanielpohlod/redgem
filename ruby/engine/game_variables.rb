# The game's own values that every player shares: what was said, what was given, how much gold the
# party has. Dialogue conditions read them, and dialogue effects change them (see dialogue.rb).
#
#   GameVariables[:gold] = 10
#   GameVariables[:gold]                                  # => 10
#   GameVariables.on_change { |name, value| Debug.log("#{name} is #{value.inspect}") }
#
# A name is a text (a symbol stands for its name). A value is nil, true, false, a number or a text;
# nil is the value of every variable never set. In a game over the network the host owns them: it
# tells every machine of each change, and a newcomer of all of them as it joins. A client reads
# them, and setting one there raises: a client changes them through the host (a dialogue's effects
# go there). They last as the levels change, until the game ends; a game saves them with
# Save.write(:variables, GameVariables.to_h) and puts them back with GameVariables.replace.
# What on_change registers lasts until the next level is loaded. The quests keep their state here too
# ("quest.<name>", see quests.rb).
module GameVariables
  MAX_NAME_SIZE = 64      # bytes in a variable's name
  MAX_TEXT_SIZE = 1024    # bytes in a text a variable holds
  MAX_COUNT = 1024        # variables at once: what one message to a newcomer carries
  @values = {}
  @handlers = []

  class << self
    # The value of a variable: nil when it was never set.
    def [](name) = @values[checked_name(name)]

    # Sets a variable, on this machine and (on the host) on every other. Raises on a client, and
    # for a name or a value a variable cannot have.
    def []=(name, value)
      raise ArgumentError, "only the host changes GameVariables in a game over the network" if client?

      change(checked_name(name), checked_value(value))
    end

    # Every variable that was set, by name.
    def to_h = @values.dup

    # Puts these in the place of every variable (Save's, read back). Raises as []= does.
    def replace(values)
      raise ArgumentError, "only the host changes GameVariables in a game over the network" if client?

      put_all(checked_all(values))
      Network.__deliver(Network::VARIABLES, @values.to_a, :all) if Network.online?
      to_h
    end

    # |name, value| on every change, here and from the host.
    def on_change(&block) = @handlers << block

    # Whether a variable can hold a value.
    def value?(value) = [nil, true, false].include?(value) || number?(value) || text?(value)

    # Whether a variable can be called so.
    def name?(name) = name.is_a?(String) && !name.empty? && name.bytesize <= MAX_NAME_SIZE

    # Called by the engine when a level is loaded.
    def __scene_reset = @handlers = []

    # The host told of a change (see network_dialogues.rb), or of all of them.
    def __told(name, value) = change(name, value, tell: false)

    def __told_all(values) = put_all(values)

    # Whether a variable can be called so and hold that (what the host says is checked so).
    def __pair?(name, value) = name?(name) && value?(value)

    private

    def client? = Network.online? && !Network.host?

    def number?(value) = value.is_a?(Numeric) && value.to_f.finite?

    def text?(value) = value.is_a?(String) && value.bytesize <= MAX_TEXT_SIZE

    def checked_name(name)
      name = name.to_s
      raise ArgumentError, "a GameVariables name is a text of 1 to #{MAX_NAME_SIZE} bytes, not #{name.inspect}" unless
        name?(name)

      name
    end

    def checked_value(value)
      return value if value?(value)

      raise ArgumentError, "GameVariables hold nil, true, false, numbers and texts, not #{value.inspect}"
    end

    # (Through to_a: mruby's Hash#to_h takes no block.)
    def checked_all(values)
      checked = values.to_a.to_h { |name, value| [checked_name(name), checked_value(value)] }
      raise ArgumentError, "GameVariables hold at most #{MAX_COUNT} variables" if checked.size > MAX_COUNT

      checked
    end

    # (A variable that is new past MAX_COUNT is refused: every one must reach a newcomer.)
    def change(name, value, tell: true)
      raise ArgumentError, "GameVariables hold at most #{MAX_COUNT} variables" if full_for?(name)

      was = @values[name]
      store(name, value)
      announce(name, value) if tell
      heard(name, value, was, false)
      value
    end

    # The quests, then the game's handlers, hear of a change (bulk: as all were put back).
    def heard(name, value, was, bulk)
      Quests.__variable(name, value, was, bulk)
      @handlers.dup.each { |block| block.call(name, value) }
    end

    def store(name, value) = value.nil? ? @values.delete(name) : @values[name] = value

    # (Every other machine hears of it, from the host.)
    def announce(name, value)
      Network.__deliver(Network::VARIABLE, [name, value], :all) if Network.online?
    end

    def full_for?(name) = !@values.key?(name) && @values.size >= MAX_COUNT

    # Every variable becomes one of these (nil: not set); the handlers hear of each that changed.
    def put_all(values)
      changed = (@values.keys | values.keys).reject { |name| @values[name] == values[name] }
      @values = values.compact
      changed.each { |name| heard(name, @values[name], nil, true) }
    end
  end
end
