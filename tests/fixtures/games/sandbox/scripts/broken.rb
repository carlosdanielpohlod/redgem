# Fails on purpose: the engine must log it, disable this behaviour and keep running.
class Broken < Behaviour
  def update(_dt)
    $broken_calls = ($broken_calls || 0) + 1
    raise "boom from Broken"
  end
end
