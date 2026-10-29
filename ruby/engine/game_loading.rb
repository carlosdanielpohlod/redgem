# A loading callback belongs to the current scene. It draws while simulation is frozen; input,
# networking and audio still advance. The default is a translated label over the old scene.
module Game
  LOADING_MARGIN = 24
  LOADING_TEXT_SIZE = 28

  def self.on_loading(&block)
    @loading_screen = block
  end

  def self.__draw_loading(delta)
    return @loading_screen.call(delta) if @loading_screen

    UI.text(I18n.t("game.loading"), x: LOADING_MARGIN, y: LOADING_MARGIN,
                                    size: LOADING_TEXT_SIZE, color: :white)
  end

  def self.__scene_reset
    @loading_screen = nil
  end
end
