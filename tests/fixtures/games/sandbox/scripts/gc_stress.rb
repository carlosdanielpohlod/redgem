# Forces a full garbage collection every frame so unrooted Ruby values held by C++ show up as crashes.
class GcStress < Behaviour
  def update(_dt)
    GC.start
  end

  def fixed_update(_dt)
    GC.start
  end
end
