# The player's car's speedometer, in the bottom right corner of the screen: the speed in km/h, the
# gear, and a bar showing how far up its speed range the car is.
class Speedometer
  PANEL_WIDTH = 230
  PANEL_HEIGHT = 104
  PANEL_MARGIN = 28      # pixels between the panel and the screen's edges
  PANEL_ALPHA = 150
  PANEL_RADIUS = 14
  SPEED_SIZE = 64
  SPEED_Y = 12           # pixels from the panel's top to the speed ...
  SPEED_RIGHT = 80       # ... and from the panel's right edge to where its digits end
  UNIT = "km/h".freeze
  UNIT_SIZE = 20
  UNIT_Y = 50
  KMH_PER_MS = 3.6
  GEAR_SIZE = 34
  GEAR_Y = 26
  REVERSE = "R".freeze   # what the gear reads while the car backs up
  BAR_HEIGHT = 6
  BAR_INSET = 18         # pixels between the bar (and the texts) and the panel's edges
  BAR_ALPHA = 110
  BAR_RADIUS = 3

  # `speed` in m/s (negative in reverse), out of a `top_speed`; `gear` as a number.
  def draw(speed, top_speed, gear)
    @x = UI.width - PANEL_WIDTH - PANEL_MARGIN
    @y = UI.height - PANEL_HEIGHT - PANEL_MARGIN
    UI.rect(x: @x, y: @y, width: PANEL_WIDTH, height: PANEL_HEIGHT, alpha: PANEL_ALPHA, radius: PANEL_RADIUS)
    draw_speed(speed.abs)
    UI.text(speed.negative? ? REVERSE : gear.to_s, x: @x + BAR_INSET, y: @y + GEAR_Y, size: GEAR_SIZE, color: :ruby)
    draw_bar(speed.abs / top_speed)
  end

  private

  def draw_speed(speed)
    right = @x + PANEL_WIDTH - BAR_INSET
    kmh = (speed * KMH_PER_MS).round.to_s
    UI.text(kmh, x: @x + PANEL_WIDTH - SPEED_RIGHT, y: @y + SPEED_Y, size: SPEED_SIZE, align: :right)
    UI.text(UNIT, x: right, y: @y + UNIT_Y, size: UNIT_SIZE, color: :gray, align: :right)
  end

  # How far up the speed range the car is: `share` from 0 to 1.
  def draw_bar(share)
    track = PANEL_WIDTH - BAR_INSET * 2
    bar = { x: @x + BAR_INSET, y: @y + PANEL_HEIGHT - BAR_INSET, height: BAR_HEIGHT, radius: BAR_RADIUS }
    UI.rect(width: track, color: :gray, alpha: BAR_ALPHA, **bar)
    filled = (track * share).to_i
    UI.rect(width: filled, color: :ruby, **bar) if filled.positive?
  end
end
