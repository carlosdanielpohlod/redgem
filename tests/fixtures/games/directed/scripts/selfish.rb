# Uses its own class before defining it: must fail with an error, not load itself forever.
Selfish.new
class Selfish < Behaviour
end
