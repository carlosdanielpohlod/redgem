# A marker floating over the coin nearest the camera, drawn where the camera shows it
# (Camera.world_to_screen): none once every coin is collected, or while that coin is behind the
# camera.
class CoinMarker < Behaviour
  HEIGHT = 0.9                    # meters above the coin's middle
  SIZE = 14                       # pixels across
  COLOR = [255, 64, 160].freeze   # no coin, sky or grass is this pink: a test finds the marker by it
  COIN_TAG = :coin

  attr_reader :coin, :spot # the coin marked, and where the marker is drawn: [x, y], nil when nowhere

  def update(_dt)
    @coin = nearest_coin
    @spot = @coin && Camera.world_to_screen(@coin.transform.position + Vector3.up * HEIGHT)
    draw if @spot
  end

  private

  def nearest_coin = Scene.find_all(COIN_TAG).min_by { |coin| coin.transform.position.distance_to(transform.position) }

  def draw
    x, y = @spot
    UI.rect(x: x - SIZE / 2, y: y - SIZE / 2, width: SIZE, height: SIZE, color: COLOR, radius: SIZE / 2)
  end
end
