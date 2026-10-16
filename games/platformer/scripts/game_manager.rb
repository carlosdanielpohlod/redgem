# Keeps the score, times the run and draws the HUD; the fastest win is kept between runs (Save).
# Lives on the "GameManager" empty in Blender.
class GameManager < Behaviour
  HUD_X = 20
  COINS_Y = 40
  HINT_Y = 76
  HUD_SIZE = 28
  HINT_SIZE = 20
  HINT_SECONDS = 8.0
  BANNER_X = 430
  BANNER_Y = 300
  BANNER_SIZE = 64
  NEED_COINS_SECONDS = 2.5
  BEST_TIME_KEY = :best_time
  BEST_RIGHT_MARGIN = 20   # pixels between the best time and the screen's right edge

  HOME = "GameManager".freeze   # the object it lives on
  COIN_TAG = :coin

  attr_reader :coins, :total_coins, :best_time, :collected

  def self.instance = Scene.find!(HOME).get_component(GameManager)

  def start
    @coins = 0
    @collected = []   # names of the coins picked up
    @total_coins = Scene.find_all(COIN_TAG).size
    @won = false
    @need_coins_until = 0.0
    @started_at = Time.elapsed
    @best_time = Save[BEST_TIME_KEY]
  end

  # Seconds played in this run (the clock stops while a menu is open, and when the game is won).
  def run_time = (@finished_at || Time.elapsed) - @started_at

  def collect_coin(name)
    @coins += 1
    @collected << name
  end

  def all_coins? = @coins >= @total_coins
  def won? = @won

  def reached_goal
    return win(mine: true) if all_coins?

    @need_coins_until = Time.elapsed + NEED_COINS_SECONDS
  end

  # The game is won: by this machine's hero (`mine`), or by another player's.
  def win(mine:)
    return if @won

    @won = true
    @finished_at = Time.elapsed
    CoOp.instance.won if mine
    record = record?
    Save[BEST_TIME_KEY] = @best_time = run_time if record
    GameMenu.instance.finish(record ? I18n.t("over.record", seconds: seconds(run_time)) : I18n.t("over.win"))
  end

  def update(_dt)
    draw_coins
    draw_best_time if @best_time
    draw_hint if run_time < HINT_SECONDS
    draw_banner
  end

  private

  # Whether the run that just ended is the fastest so far.
  def record? = @best_time.nil? || run_time < @best_time

  # A time as the texts show it: 12.3.
  def seconds(time) = format("%<seconds>.1f", seconds: time)

  def draw_coins
    UI.text(I18n.t("hud.coins", coins: @coins, total: @total_coins), x: HUD_X, y: COINS_Y, size: HUD_SIZE, color: :gold)
  end

  def draw_best_time
    text = I18n.t("hud.best", seconds: seconds(@best_time))
    UI.text(text, x: UI.width - BEST_RIGHT_MARGIN, y: COINS_Y, size: HUD_SIZE, align: :right)
  end

  def draw_hint
    UI.text(I18n.t("hud.hint"), x: HUD_X, y: HINT_Y, size: HINT_SIZE)
  end

  def draw_banner
    return UI.text(I18n.t("banner.win"), x: BANNER_X, y: BANNER_Y, size: BANNER_SIZE, color: :yellow) if @won
    return unless Time.elapsed < @need_coins_until

    UI.text(I18n.t("banner.need_coins"), x: BANNER_X, y: BANNER_Y, size: HUD_SIZE, color: :orange)
  end
end
