# Loading a level. A loading callback belongs to the current scene. It draws while simulation is frozen; input,
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

  # On a network game, the level is the host's to change: every machine loads it in the
  # background, and the world waits until all of them have it (see network_levels.rb).
  class << self
    # Switches to scenes/<name>.glb ("main" is scene.glb).
    def load_scene(name)
      return __load_scene(name) unless __online?

      Network::Level.ask(name.to_s)
    end

    def load_scene_async(name)
      return __load_scene_async(name) unless __online?

      Network::Level.ask(name.to_s)
    end

    # A change of level with the others is not cancelled: false, as nothing was.
    def cancel_scene_load
      return false if __held?

      __cancel_scene_load
    end
  end
end
