# What a game keeps between runs (the engine's natives are Save.__get, __set...), and which version
# of the game's saved data a slot holds. A shipped game that changes what it saves says so, and
# says how an older slot becomes the new one:
#
#   Save.version = 3
#   Save.migrate(from: 1) { |data| data.merge("coins" => data.delete("gold")) }
#   Save.migrate(from: 2) { |data| data.merge("lives" => 3) }
#
# The first use of a slot (or Save.slot = name) checks it: an older one goes through the hooks in
# order and is written back whole, at once, stamped with the new version, so each hook runs once.
# A slot written by a newer game, or one a hook failed on, is refused: it stays as it was, and every
# use of it raises, saying why.
module Save
  VERSION_KEY = "__save_version".freeze
  VERSION_SYMBOL = VERSION_KEY.to_sym
  # What a slot written before its game declared a version is.
  FIRST_VERSION = 1

  @version = nil
  @migrations = {}
  @checked = false
  @refusal = nil
  @stamped = false

  # The version of the saved data this game writes (nil: none declared, slots are taken as they are).
  def self.version = @version

  def self.version=(number)
    __require_version(number, "Save.version")
    @version = number
    __recheck
  end

  # What turns a slot of version `from` into one of version from + 1: the block is given the slot's
  # values (a Hash with String keys) and returns the new ones. Versions that changed nothing need none.
  def self.migrate(from:, &hook)
    raise ArgumentError, "Save.migrate needs a block making the new values" unless hook

    __require_version(from, "Save.migrate from:")
    raise ArgumentError, "Save.migrate from: #{from} is declared twice" if @migrations.key?(from)

    @migrations[from] = hook
  end

  def self.get(key, default = nil)
    __ready
    __get(key, default)
  end

  def self.[](key) = get(key)

  def self.set(key, value)
    __writing
    __set(key, value)
  end

  def self.[]=(key, value)
    set(key, value)
  end

  def self.update(key, default = nil, &)
    __writing
    __update(key, default, &)
  end

  def self.delete(key)
    __writing
    __delete(key)
  end

  def self.clear
    __ready
    __clear
    @stamped = false
  end

  def self.keys
    __ready
    __keys.reject { |key| key == VERSION_SYMBOL }
  end

  # Selects a slot and checks it; a refused one leaves the selection where it was, and raises.
  def self.slot=(name)
    previous = slot
    __select(name)
    __recheck
    __ready
  rescue RuntimeError
    __back_to(previous)
    raise
  end

  def self.__back_to(previous)
    return if previous == slot

    __select(previous)
    __recheck
  end

  def self.__require_version(number, what)
    return if __valid_version?(number)

    raise ArgumentError, "#{what} must be a whole number of #{FIRST_VERSION} or more, not #{number.inspect}"
  end

  def self.__valid_version?(number) = number.is_a?(Integer) && number >= FIRST_VERSION

  def self.__recheck
    @checked = false
    @refusal = nil
  end

  def self.__ready
    __check unless @checked
    raise @refusal if @refusal
  end

  def self.__writing
    __ready
    __stamp if __unstamped?
  end

  # (A slot holds its version from its first write on.)
  def self.__unstamped? = @version && !@stamped

  def self.__stamp
    __set(VERSION_KEY, @version)
    @stamped = true
  end

  # (Nothing is checked until the game declares a version.)
  def self.__check
    return unless @version

    @checked = true
    stored = __get(VERSION_KEY)
    @stamped = !stored.nil?
    return if __fresh?(stored)

    __settle(stored || FIRST_VERSION)
  end

  def self.__fresh?(stored) = stored.nil? && __keys.empty?

  def self.__settle(stored)
    return __refuse("holds no valid save version (#{stored.inspect})") unless stored.is_a?(Integer)
    return __refuse(__newer(stored)) if stored > @version

    __upgrade(stored) if stored < @version
  end

  def self.__newer(stored)
    "was saved by a newer version of the game (save version #{stored}, this one is #{@version}): it is not loaded"
  end

  # (Under the save's lock, as Save.update: another room of a server may have migrated it meanwhile.)
  def self.__upgrade(from)
    __update(VERSION_KEY) { |stored| __upgraded(stored, stored || from) }
    @stamped = @refusal.nil?
  end

  # What the slot's version becomes: the new one once the migrated values are written, else what it was.
  def self.__upgraded(stored, from)
    return stored unless from < @version

    data = (from...@version).inject(__values) { |values, step| __migrated(values, step) }
    __replace(data.merge(VERSION_KEY => @version))
    @version
  rescue StandardError => e
    __refuse("could not be migrated to save version #{@version}: #{e.class}: #{e.message}; it is left as it was")
    stored
  end

  def self.__values
    __keys.each_with_object({}) { |key, values| values[key.to_s] = __get(key) unless key == VERSION_SYMBOL }
  end

  def self.__migrated(values, from)
    hook = @migrations[from]
    return values unless hook

    made = hook.call(values)
    raise TypeError, "Save.migrate(from: #{from}) must return the new values as a Hash" unless made.is_a?(Hash)

    made.each_with_object({}) { |(key, value), strings| strings[key.to_s] = value }
  end

  def self.__refuse(why)
    @refusal = "save slot '#{slot}' #{why}"
    Debug.warn(@refusal)
  end
end
