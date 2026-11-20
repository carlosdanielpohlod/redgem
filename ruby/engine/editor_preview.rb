# The editor's Story panel (src/editor/story_preview.hpp): a dialogue or a quest run without Play,
# in an engine of its own with no scene, whose GameVariables are scratch ones. Each call answers
# with where the conversation is, the quests that started and every variable, as JSON text. The
# engine asks; a game has no use for it.
module EditorPreview
  class << self
    # A new conversation of a dialogue, read again from its file (ending the one before).
    def __dialogue(name)
      forget_files
      @conversation = Dialogue::Conversation.new(Dialogue.load(name), actor: nil, deciding: true)
      @conversation.begin
      __state
    end

    # Takes a choice of the line (its index among the node's choices, as text), or goes on from a
    # line with none.
    def __choose(index) = act { @conversation&.choose(Integer(index)) }
    def __continue = act { @conversation&.continue }

    # Starts a quest at its first stage (read again from its file, and from nothing when it had
    # started), or moves it to a stage.
    def __start_quest(name)
      forget_files
      act { Quests.set_stage(name, Quests.load(name).start) }
    end

    def __set_stage(name, stage)
      forget_files
      act { Quests.set_stage(name, stage) }
    end

    def __state = HTTP.__json({ dialogue: conversation_state, quests: quests_state, variables: GameVariables.to_h })

    private

    def act
      yield
      __state
    end

    # The dialogues and quests read so far: the next step reads their files again.
    def forget_files
      Dialogue.__scene_reset
      Quests.__scene_reset
    end

    def conversation_state
      return nil unless @conversation

      node = @conversation.node || {}
      { name: @conversation.script.name, node: @conversation.node_id, over: @conversation.over?,
        speaker: @conversation.speaker_name, text: @conversation.text, text_key: @conversation.text_key,
        effects: node["effects"] || [], next: node["next"], choices: choices_of(node) }
    end

    # Every choice of the line, those whose conditions do not hold too.
    def choices_of(node)
      (node["choices"] || []).each_with_index.map do |choice, index|
        { index: index, text: I18n.t(choice["text"]), if: choice["if"] || [], effects: choice["effects"] || [],
          next: choice["next"], offered: Dialogue::Rules.hold?(choice["if"]) }
      end
    end

    def quests_state = Quests.started.map { |quest| quest_state(quest) }

    # A quest's stages from its start along each one's "next", then the others.
    def story_order(definition)
      chained = chain(definition, definition.start, [])
      chained + (definition.stage_ids - chained)
    end

    def chain(definition, id, ordered)
      return ordered unless next_in_story?(definition, id, ordered)

      chain(definition, definition.stage(id)["next"], ordered << id)
    end

    def next_in_story?(definition, id, ordered) = definition.stage?(id) && !ordered.include?(id)

    def quest_state(quest)
      { name: quest.name, stage: quest.stage, complete: quest.complete?, text: quest.stage_text,
        next: quest.next_stage, stages: quest.definition ? story_order(quest.definition) : [],
        objectives: quest.objectives.map { |objective| { text: objective.text, done: objective.done? } } }
    end
  end
end
