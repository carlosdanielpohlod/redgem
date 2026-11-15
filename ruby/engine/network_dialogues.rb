# Conversations and GameVariables over the network (see dialogue.rb and game_variables.rb). The
# host starts a player's conversation on that player's machine and follows it there: the client
# says each step it takes, the host takes the same step on its copy (where the effects happen), and
# says where the client should be when it decided otherwise. The host tells everyone of every
# change of a GameVariable, and a newcomer of all of them.
module Network
  class << self
    # The host starts a conversation for the player of another machine (see Dialogue.start). True.
    def __talk(peer, script, actor, speaker)
      Dialogue.__follow(peer, script, actor, speaker)
      __deliver(TALK, [script.name, __key_of(actor), speaker_key(speaker)], peer)
      true
    end

    private

    # What the client finds the speaker by; nil for one it could not (a name too long or empty).
    def speaker_key(speaker)
      key = speaker && __key_of(speaker)
      Said.object_key?(key) ? key : nil
    end

    # (From the host only: no other machine starts conversations.)
    def receive_talk(data, from)
      return unless from == HOST && Said.talk?(data)

      script = dialogue_here(data[0])
      return unless script

      Dialogue.__here(script, @synced[data[1]]&.game_object, speaker_called(data[2]), deciding: false) do |*step|
        tell_host(data[0], *step)
      end
    end

    # The dialogue the host named, as this machine's file has it. One it cannot read is said in the
    # log, and the host forgets the conversation.
    def dialogue_here(name)
      Dialogue.load(name)
    rescue ArgumentError => e
      Debug.error(e.message)
      __deliver(TALK_LEFT, [name], :host)
      nil
    end

    def speaker_called(key) = key && objects_called(key).first

    # A client's conversation moved: the host's copy takes the same step.
    def tell_host(name, from, choice, to)
      return __deliver(TALK_LEFT, [name], :host) if choice == :left

      __deliver(STEP, [name, from, choice, to], :host)
    end

    def receive_step(data, from)
      mirror = Dialogue.__mirror_of(from)
      return unless host? && Said.step?(data) && following?(mirror, data[0])

      follow_step(mirror, from, *data[1..])
    end

    def following?(mirror, name) = !mirror.nil? && mirror.script.name == name

    # The step must start where the host's copy is, and lead where it leads there.
    def follow_step(mirror, peer, from, choice, to)
      moved = mirror.node_id == from && take_step(mirror, choice)
      __deliver(TALK_AT, [mirror.script.name, mirror.node_id], peer) unless moved && mirror.node_id == to
      Dialogue.__forget(peer) if mirror.over?
    end

    def take_step(mirror, choice) = choice.nil? ? mirror.continue : mirror.choose(choice)

    # (The host may put back a conversation that ended here too soon: its effects had not come yet.)
    def receive_talk_at(data, from)
      conversation = Dialogue.current || Dialogue.__last
      return unless from == HOST && Said.talk_at?(data) && following?(conversation, data[0])

      Dialogue.__resume(conversation, data[1])
    end

    def receive_talk_left(data, from)
      Dialogue.__forget(from) if host? && Said.talk_left?(data) && following?(Dialogue.__mirror_of(from), data[0])
    end

    def receive_variable(data, from)
      GameVariables.__told(*data) if from == HOST && Said.list?(data, Said::PAIR_SIZE) && GameVariables.__pair?(*data)
    end

    def receive_variables(data, from)
      GameVariables.__told_all(data.to_h) if from == HOST && Said.variables?(data)
    end

    # The host tells a newcomer every GameVariable.
    def tell_variables(peer)
      __deliver(VARIABLES, GameVariables.to_h.to_a, peer) if host?
    end

    def forget_talks_of(peer) = Dialogue.__forget(peer)

    def forget_all_talks(_peer) = Dialogue.__network_gone
  end

  # What machines say of conversations and variables, checked before it is used.
  module Said
    MAX_NODE_SIZE = 128   # bytes in the id of a dialogue's node, as a step carries it
    TALK_SIZE = 3
    STEP_SIZE = 4
    TALK_AT_SIZE = 2
    PAIR_SIZE = 2

    class << self
      def talk?(data) = list?(data, TALK_SIZE) && dialogue?(data[0]) && object_key?(data[1]) && no_or_key?(data[2])

      def step?(data) = list?(data, STEP_SIZE) && dialogue?(data[0]) && step_parts?(*data[1..])

      def talk_at?(data) = list?(data, TALK_AT_SIZE) && dialogue?(data[0]) && no_or_node?(data[1])

      def talk_left?(data) = list?(data, 1) && dialogue?(data[0])

      def variables?(data)
        data.is_a?(Array) && data.size <= GameVariables::MAX_COUNT && data.all? { |pair| variable?(pair) }
      end

      private

      def dialogue?(name) = object_key?(name)

      def node?(id) = id.is_a?(String) && !id.empty? && id.bytesize <= MAX_NODE_SIZE

      def no_or_node?(id) = id.nil? || node?(id)

      def no_or_key?(key) = key.nil? || object_key?(key)

      def step_parts?(from, choice, to) = node?(from) && no_or_index?(choice) && no_or_node?(to)

      def no_or_index?(choice) = choice.nil? || (choice.is_a?(Integer) && choice >= 0)

      def variable?(pair) = list?(pair, PAIR_SIZE) && GameVariables.__pair?(*pair)
    end
  end
end
