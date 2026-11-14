# Conversations with the people of a game, written in dialogue files: dialogues/<name>.json, each a
# set of lines (nodes) that lead to one another, some with choices for the player. Their texts are
# I18n keys; conditions and effects read and change GameVariables and quest stages:
#
#   {
#     "start": "hello",
#     "nodes": {
#       "hello": { "speaker": "baker.name", "text": "baker.hello", "voice": "baker/hello.ogg",
#                  "choices": [
#                    { "text": "baker.buy", "if": [{ "var": "gold", "at_least": 5 }],
#                      "effects": [{ "add": "gold", "by": -5 }, { "set": "has_bread", "to": true }],
#                      "next": "thanks" },
#                    { "text": "baker.bye", "end": true } ] },
#       "thanks": { "text": "baker.thanks", "effects": [{ "quest": "bread", "to": "done" }], "end": true }
#     }
#   }
#
# A node has a "text", maybe a "speaker" (the key of the name shown) and a "voice" (a sound kept for
# later: nothing plays it yet), and goes on in one way: "next" (a node, or a list of branches, the
# first whose "if" holds: [{ "if": [...], "next": "a" }, { "next": "b" }]), "choices", or
# "end": true. "start" goes as "next" does. Its "effects" happen as it is reached; a choice's, as it
# is chosen. A condition reads a { "var": name } or a { "quest": name } and holds when the value
# "is", is "not", is "at_least" or "at_most" something. An effect does { "set": name, "to": value },
# { "add": name, "by": number }, { "quest": name, "to": stage }, or { "event": name, "data": value },
# which Dialogue.on(:event) hears of. `redgem check` reads every dialogue file (see
# docs/scripting/dialogue.md).
#
# A conversation starts from on_interact (see Talkable, which does it), on the host in a game over
# the network: Dialogue.start runs it on the machine of the player whose hero it is, as that
# player's own. Its effects happen on the host, where it is followed step by step: a client cannot
# say it chose what it was not offered.
module Dialogue
  EVENTS = %i[start line end event].freeze
  @handlers = {}
  @scripts = {}
  @mirrors = {}   # on the host: the conversations of the other players, by their machine's id

  class << self
    # What reads and changes quest stages: an object with stage(name) and set_stage(name, stage,
    # conversation). Quests that are GameVariables ("quest.<name>") unless a game says.
    attr_writer :quests

    def quests = @quests || VariableQuests

    # The conversation this machine's player is in, or nil.
    attr_reader :current

    # The dialogue of that name, read from its file once. Raises for one that is not there or does
    # not read (saying where).
    def load(name) = @scripts[name.to_s] ||= Script.new(name.to_s, __read(name.to_s))

    # Starts a conversation of `actor` (the hero of a player) with `speaker` (who talks to it), on
    # the machine of that player (ending the one it was in). The conversation, or true when it went
    # to another machine. Raises on a client: the host starts them (from on_interact).
    def start(name, actor:, speaker: nil)
      raise ArgumentError, "Dialogue.start runs on the host in a game over the network" if client?

      script = name.is_a?(Script) ? name : load(name)
      peer = player_of(actor)
      return Network.__talk(peer, script, actor, speaker) if peer

      __here(script, actor, speaker, deciding: true)
    end

    # Ends the conversation this machine's player is in.
    def stop = @current&.stop

    # :start, :line and :end |conversation| (this machine's player's), and :event |name, data,
    # conversation| (where effects happen: the host).
    def on(event, &block)
      raise ArgumentError, "Dialogue.on takes #{EVENTS.inspect}, not #{event.inspect}" unless EVENTS.include?(event)

      (@handlers[event] ||= []) << block
    end

    # The host's copy of another player's conversation, or nil.
    def __mirror_of(peer) = @mirrors[peer]

    # Starts this machine's player's conversation. deciding: its effects happen here.
    def __here(script, actor, speaker, deciding:, &stepped)
      @current&.stop(quietly: true)
      @current = Conversation.new(script, actor: actor, speaker: speaker, deciding: deciding, shown: true, &stepped)
      tell(:start, @current)
      @current.begin
    end

    # The host follows another player's conversation, to make its effects happen.
    def __follow(peer, script, actor, speaker)
      @mirrors[peer] = Conversation.new(script, actor: actor, speaker: speaker, deciding: true, shown: false)
      @mirrors[peer].begin
    end

    def __forget(peer) = @mirrors.delete(peer)

    # The game over the network is gone: the other players' conversations, and this one's if it
    # was another machine's to decide.
    def __network_gone
      @mirrors = {}
      @current&.stop(quietly: true) unless @current&.deciding?
    end

    # Called by a conversation as it moves on and ends.
    def __shown(event, conversation)
      @current = nil if event == :end && @current.equal?(conversation)
      tell(event, conversation)
    end

    def __event(name, data, conversation) = tell(:event, name, data, conversation)

    # Called by the engine when a level is loaded: every conversation ends with it.
    def __scene_reset
      @current&.stop(quietly: true)
      @handlers = {}
      @scripts = {}
      @mirrors = {}
    end

    private

    def tell(event, *values) = (@handlers[event] || []).dup.each { |block| block.call(*values) }

    def client? = Network.online? && !Network.host?

    # The machine whose player plays the actor, when it is another one.
    def player_of(actor)
      sync = actor&.get_component(NetworkSync)
      return nil if sync.nil? || !Network.online? || sync.mine?

      sync.owner
    end
  end

  # Quest stages kept as GameVariables, "quest.<name>", until the game has quests of its own.
  module VariableQuests
    PREFIX = "quest.".freeze

    def self.stage(name) = GameVariables[PREFIX + name.to_s]

    def self.set_stage(name, stage, _conversation)
      GameVariables[PREFIX + name.to_s] = stage
    end
  end
end
