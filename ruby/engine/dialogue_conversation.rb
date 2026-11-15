# Dialogue::Conversation: one player's way through a dialogue (see dialogue.rb). It is at a node
# (a line, maybe with choices) until it is told to go on, and then over.
#
#   conversation = Dialogue.current
#   conversation.text                     # the line, in the language in use
#   conversation.choices.map(&:text)      # what the player may answer (those whose "if" holds)
#   conversation.choose(conversation.choices.first)
#   conversation.continue                 # a line with no choices: on to what follows
#   conversation.speaking?                # its line's voice clip plays
#
# A conversation that decides makes the effects happen (offline, and on the host); the others
# (a client's) only move, and the host, which follows them, makes them happen there.
module Dialogue
  class Conversation
    # One answer the player may give: its index among the node's choices, and its text.
    class Choice
      attr_reader :index, :key   # key: the I18n key of its text

      def initialize(index, key)
        @index = index
        @key = key
      end

      def text = I18n.t(key)
    end

    attr_reader :script, :actor, :speaker, :node_id

    # stepped: |from, choice, to| after every move (choice: its index, nil, or :left when the player
    # left it with stop; to: nil at the end).
    def initialize(script, actor:, speaker: nil, deciding: true, shown: false, &stepped)
      @script = script
      @actor = actor
      @speaker = speaker
      @deciding = deciding
      @shown = shown
      @stepped = stepped
    end

    # Goes to where the dialogue starts. Itself.
    def begin
      @started = true
      enter(Rules.target(script.start))
      self
    end

    def deciding? = @deciding

    def over? = @started && @node_id.nil?

    # The node it is at (a Hash), or nil once it is over.
    def node = @node_id && script.node(@node_id)

    def text_key = node&.fetch("text")
    def speaker_key = node && node["speaker"]
    # The sound of its line's voice clip (in sounds/), or nil.
    def voice = node && node["voice"]

    # Whether its line's voice plays (from the speaker's head, in the :voice group). A line with no
    # choices goes on by itself when its clip ends; continue skips it.
    def speaking? = @voice&.playing? || false

    def text = text_key && I18n.t(text_key)
    def speaker_name = speaker_key && I18n.t(speaker_key)

    # Whether the player answers this line by choosing (else continue goes on).
    def choosing? = !node.nil? && node.key?("choices") && !choices.empty?

    # The choices the player may take now: those whose conditions hold.
    def choices
      listed = (node && node["choices"]) || []
      offered = listed.each_index.select { |index| Rules.hold?(listed[index]["if"]) }
      offered.map { |index| Choice.new(index, listed[index]["text"]) }
    end

    # On from a line the player does not answer: to what follows, or the end (also from a line whose
    # choices all have conditions that do not hold). False when it waits for a choice, or is over.
    def continue
      return false if over? || choosing?

      move(nil, where_next(node))
    end

    # Takes a choice (a Choice, or its index): its effects happen, then the conversation goes where
    # it leads. False for one that is not offered now.
    def choose(choice)
      index = choice.is_a?(Choice) ? choice.index : choice
      return false unless offered?(index)

      taken = node["choices"][index]
      Rules.apply(taken["effects"], self) if @deciding
      move(index, where_next(taken))
    end

    # Ends it where it is: the player left it. quietly: without telling (stepped) of it, as when
    # another conversation takes its place or the level goes.
    def stop(quietly: false)
      return false if over?

      from = @node_id
      enter(nil)
      @stepped&.call(from, :left, nil) unless quietly
      true
    end

    # Where the host says it is (the host decided otherwise): no effects happen here.
    def __jump(node_id) = enter(script.node?(node_id) ? node_id : nil)

    # Called every frame on the conversation shown: the voice follows the speaker, and the line goes
    # on when its clip ends.
    def __update
      return if @voice.nil?

      @voice.follow
      return if @voice.playing?

      @voice = nil
      continue
    end

    def inspect = "#<Dialogue::Conversation #{script.name} at #{@node_id.inspect}>"

    private

    def offered?(index) = choices.any? { |offered| offered.index == index }

    # Where a node or a choice leads: a node's id, or nil (the end).
    def where_next(step) = step.key?("next") ? Rules.target(step["next"]) : nil

    def move(choice, to)
      from = @node_id
      enter(to)
      @stepped&.call(from, choice, to)
      true
    end

    def enter(node_id)
      @node_id = node_id
      return finish if node_id.nil?

      Rules.apply(node["effects"], self) if @deciding
      speak if @shown
      Dialogue.__shown(:line, self) if @shown
    end

    # The line's voice plays, in place of the one before.
    def speak
      silence
      @voice = voice && Voice.play(voice, speaker)
    end

    def silence
      @voice&.stop
      @voice = nil
    end

    def finish
      @started = true
      silence
      Dialogue.__shown(:end, self) if @shown
    end
  end
end
