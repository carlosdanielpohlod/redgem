# Changing level together: in a network game, the host decides the level and every machine loads
# it, and nobody plays it before all of them have it (see Game.load_scene).
#
#   Game.load_scene("level2")                          # on the host (a player's, or a dedicated server)
#   Network.on_level_loading { |name| show_banner }    # every machine, as the change starts
#   Network.on_level_ready { |name| start_round }      # every machine, as the new level starts
#   Network.level_timeout = 20                         # seconds to wait for the slowest machine
#
# A machine late past the timeout is let go (on_leave, there; on_disconnect, on that machine).
# The handlers last across levels, like Network.authenticate: each call replaces the one before.
module Network
  # (What the machines say to each other about it: LEVEL, LEVEL_PLAYED, LEVEL_READY and LEVEL_GO,
  # see network.rb.) A machine that joins is told the level the game plays, and loads it unless it
  # has it already.
  LEVEL_TIMEOUT = 30.0         # seconds the host waits for every machine to have the new level

  # One level change at a time, on this machine. It lasts across the level it changes, unlike the
  # rest of Network's bookkeeping.
  module Level
    # What waits to be sent and arrives to be dropped while the machines are between two levels:
    # what is said about the objects of one level, which the other does not have.
    HELD = [SYNC, SPAWN, SPAWNED, DESPAWN].freeze

    @count = 0   # the changes the host started (each one's number)

    class << self
      attr_accessor :loading_handler, :ready_handler

      def changing? = !@name.nil?

      # Whether what is said in a message of that name waits, or is dropped, now.
      def holds?(name) = changing? && HELD.include?(name)

      # A message about objects of the next level, which waits until every machine has it.
      def hold_back(*message) = @held_back << message

      # The host asks for a level (Game.load_scene); true when the change started.
      def ask(name)
        return refuse(name) unless Network.host?

        start(@count += 1, name)
        Network.__deliver(LEVEL, [@count, name], :all)
        true
      end

      # The host said to load a level.
      def heard(change, name)
        start(change, name)
      rescue ArgumentError, TypeError => e
        give_up(e.message)
      end

      # The host tells a newcomer the level the game plays (or is changing to).
      def welcome(peer)
        Network.__deliver(LEVEL_PLAYED, [@count, @name || Game.scene_name], peer) if Network.host?
      end

      # This machine just joined a game that plays `name`: it loads that level unless it has it.
      def joined(change, name)
        return heard(change, name) unless Game.scene_name == name

        Network.__deliver(LEVEL_READY, change, :host)
      end

      # The old level is gone, here: what is shown from now on is the new one's. (What waited to be
      # said until then was about the old level's objects.)
      def arrived
        return unless changing?

        @arrived = true
        @held_back = []
      end

      # A frame while the world is held: the level is waited for, then everyone else.
      def tick(delta)
        return unless changing?

        @waited += delta
        return failed_to_load if failed?

        wait_for_the_others if here?
      end

      # A machine said it has the level of change `change` (on the host).
      def ready(change, from)
        @ready << from if waiting_on?(change)
      end

      # Whether a newcomer's word is about the level the game plays, no change going on (on the host).
      def played?(change) = !changing? && change == @count

      # The host says everyone has the level of change `change`.
      def go(change)
        finish if going?(change)
      end

      # The game is gone: nothing is waited for any more.
      def abandon
        Game.__hold(false) if changing?
        @name = nil
      end

      private

      def refuse(name)
        Debug.warn("Game.load_scene(#{name.inspect}): only the host changes the level of a network game")
        false
      end

      def start(change, name)
        Game.__load_scene_async(name)
        @change = change
        @name = name
        @arrived = false
        @said_ready = false
        @waited = 0.0
        begin_waiting
      end

      def begin_waiting
        @ready = []
        @waiting = Network.peers
        @held_back = []
        Game.__hold(true)
        loading_handler&.call(@name)
      end

      def failed? = !@arrived && !Game.loading?
      def here? = @arrived && !Game.loading?
      # Whether a machine's word is about the change the host waits on.
      def waiting_on?(change) = changing? && change == @change
      # Whether the host's word to play is about the level this machine has.
      def going?(change) = change == @change && here?
      # Whether nobody is waited for any more: none is late, or the time is up.
      def time_to_go?(late) = late.empty? || @waited >= Network.level_timeout

      def failed_to_load = give_up(Game.loading_error || "the level did not load")

      # This machine has the level: the host waits for the others, the others tell it.
      def wait_for_the_others = Network.host? ? decide : say_ready

      # A machine that cannot load the level leaves the game (a host, ending it).
      def give_up(why)
        Debug.error("cannot change level with the others: #{why}")
        Network.leave
      end

      def say_ready
        return if @said_ready

        @said_ready = true
        Network.__deliver(LEVEL_READY, @change, :host)
      end

      # The host: once everyone has the level, or the time is up, it plays without the late ones.
      def decide
        late = (@waiting & Network.peers) - @ready
        return unless time_to_go?(late)

        late.each { |peer| Network.__dismiss(peer) }
        Network.__deliver(LEVEL_GO, @change, :all)
        finish
      end

      def finish
        name = @name
        held = @held_back
        abandon
        held.each { |message| Network.__deliver(*message) }
        ready_handler&.call(name)
      end
    end
  end

  class << self
    attr_writer :level_timeout

    def level_timeout = @level_timeout || LEVEL_TIMEOUT

    # Whether the machines are between two levels: one loading, or waiting for the others.
    def changing_level? = Level.changing?

    # |name|: the host started a change of level (on the old level, before it goes).
    def on_level_loading(&block) = Level.loading_handler = block

    # |name|: every machine has the new level, which starts now.
    def on_level_ready(&block) = Level.ready_handler = block

    # Called by the engine every frame the world is held for a level change.
    def __level_tick(delta) = Level.tick(delta)

    private

    def receive_level(data, from)
      Level.heard(*data) if level_order?(data, from)
    end

    def receive_level_played(data, from)
      Level.joined(*data) if level_order?(data, from)
    end

    # Whether a change of level comes from the one who decides it (only the host), as it should be.
    def level_order?(data, from) = from == HOST && !host? && Said.level?(data)

    # A machine has the level: one the others wait for, or a newcomer, then shown what is spawned.
    def receive_level_ready(change, from)
      return unless host?
      return Level.ready(change, from) unless Level.played?(change)

      __deliver(LEVEL_GO, change, from)
      send_spawned_to(from)
    end

    def receive_level_go(change, from)
      Level.go(change) if from == HOST
    end

    def forget_game(peer)
      forget_players(peer)
      Level.abandon
    end
  end
end
