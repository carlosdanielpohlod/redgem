# On-screen caption with the traffic light state.
class Caption < Behaviour
  MARGIN = 20
  TITLE_Y = 20
  STATUS_Y = 56
  TITLE_SIZE = 28
  STATUS_SIZE = 20
  CREDITS_Y_FROM_BOTTOM = 40
  WINDOW_HEIGHT = 720
  LABELS = { "avenue" => "Avenue: green", "street" => "Street: green" }.freeze

  def update(_dt)
    UI.text("RedGem City", x: MARGIN, y: TITLE_Y, size: TITLE_SIZE)
    light = TrafficController.instance&.green_axis
    UI.text(LABELS.fetch(light, "All red"), x: MARGIN, y: STATUS_Y, size: STATUS_SIZE, color: light ? :green : :red)
    UI.text("Models: Kenney (CC0)", x: MARGIN, y: WINDOW_HEIGHT - CREDITS_Y_FROM_BOTTOM, size: STATUS_SIZE,
                                    color: :gray)
  end
end
