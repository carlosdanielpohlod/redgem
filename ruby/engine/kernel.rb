# Engine-wide Ruby constants.
module RedGem
  VERSION = "0.1.0"
end

class Module
  # A constant nobody defined yet may be a class from the game's scripts: the first use of
  # `Pedestrian` loads scripts/pedestrian.rb, so scripts never have to require each other.
  def const_missing(name)
    return Object.const_get(name) if __require_class(name.to_s)

    raise NameError.new("uninitialized constant #{name}", name)
  end
end
