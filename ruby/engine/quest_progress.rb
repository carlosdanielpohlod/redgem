# How quests move on (see quests.rb): what the players do is matched to the objectives that wait
# for it, which count it; a stage whose objectives are all done goes on to the next. All of it
# happens where the quests are decided (offline, or on the host); the other machines hear of it
# through GameVariables, and their handlers from there.
module Quests
  class << self
    private

    # (On a client, the host is told and counts it.)
    def advance(kind, subject, extra, count)
      raise ArgumentError, "a count is a whole number of 1 to #{MAX_COUNT}, not #{count.inspect}" unless count?(count)
      return Network.__deliver(Network::QUEST_REPORT, [kind, subject, extra, count], :host) if client?

      __report(kind, subject, extra, count)
      true
    end

    def count?(count) = count.is_a?(Integer) && count.between?(1, MAX_COUNT)

    # The objectives under way of the active quests for which the block holds.
    def matching(&)
      active.flat_map(&:objectives).reject(&:done?).select(&)
    end

    def did?(objective, kind, subject, extra)
      objective.kind.to_s == kind && target?(objective, subject) && recipient?(objective, extra)
    end

    # An item by its name; a place or someone by theirs, or by that of a copy of them ("Wolf.001").
    def target?(objective, subject)
      ITEM_KINDS.include?(objective.kind) ? objective.target == subject : same_name?(subject, objective.target)
    end

    def recipient?(objective, extra) = objective.kind != :deliver || same_name?(extra, objective.recipient)

    def talked_to?(objective, conversation)
      objective.kind == :talk_to && same_name?(conversation.speaker&.name, objective.target) &&
        in_dialogue?(objective, conversation) && at_node?(objective, conversation)
    end

    def in_dialogue?(objective, conversation)
      objective.dialogue.nil? || objective.dialogue == conversation.script.name
    end

    def at_node?(objective, conversation) = objective.node.nil? || objective.node == conversation.node_id

    # Whether an object called `name` is the one an objective names, or a copy of it that Blender
    # numbered: "Wolf", "Wolf.001".
    def same_name?(name, wanted)
      return false unless name.is_a?(String)
      return true if name == wanted

      name.start_with?("#{wanted}.") && digits?(name[(wanted.size + 1)..])
    end

    def digits?(text) = !text.empty? && text.each_char.all? { |char| char.between?("0", "9") }

    def add(objective, count)
      return unless objective.__current?

      reached = [objective.progress + count, objective.count].min
      GameVariables[progress_key(objective.quest.name, objective.index)] = reached
    end

    def settle_all = active.each { |quest| settle(quest.name) }

    # A quest whose objectives are all done goes on.
    def settle(name)
      quest = quest_named(name)
      move(name, quest.next_stage) if ready?(quest)
    end

    def ready?(quest) = quest.active? && !quest.next_stage.nil? && quest.objectives.all?(&:done?)

    # To a stage, its objectives from nothing; checks that are passed already count at once.
    def move(name, stage)
      @steps = (@steps || 0) + 1
      raise ArgumentError, "quest #{name} went through #{MAX_STEPS} stages at once: they go round" if @steps > MAX_STEPS

      clear_progress(name)
      GameVariables[PREFIX + name] = stage
      poll(name)
    ensure
      @steps -= 1
    end

    def clear_progress(name)
      mark = "#{PREFIX}#{name}#{PROGRESS_MARK}"
      GameVariables.to_h.each_key { |key| GameVariables[key] = nil if key.start_with?(mark) }
    end

    def poll(name)
      quest_named(name).objectives.each { |objective| add(objective, 1) if passes?(objective) }
      settle(name)
    end

    def passes?(objective) = objective.__current? && objective.kind == :check && !objective.done? && passed?(objective)

    def passed?(objective)
      check = @checks[objective.target]
      return false unless check

      check.call(objective.quest) ? true : false
    end

    # What this machine's handlers hear of a quest's stage, or an objective's progress.
    def moved(quest, stage, was)
      return if stage == was

      tell(:start, quest) if was.nil?
      tell(:stage, quest, stage, was)
      tell(:complete, quest) if quest.complete?
      tell(:change, quest)
    end

    # (As a stage starts, its objectives go back to nothing: only progress is heard of.)
    def progressed(quest, index, value)
      objective = quest.objectives[index]
      tell(:objective, quest, objective) if objective && !value.nil?
      tell(:change, quest)
    end

    # The GameVariables a saved quest becomes; none for one whose file no longer has its stage.
    def variables_of(name, state)
      stage = field(state, "stage")
      return {} unless restorable?(name, stage)

      progress = field(state, "progress")
      counts = progress.is_a?(Array) ? progress : []
      saved_progress(name, counts).merge(PREFIX + name => stage)
    end

    def saved_progress(name, counts)
      kept = counts.each_index.select { |index| count?(counts[index]) }
      kept.to_h { |index| [progress_key(name, index), counts[index]] }
    end

    def field(state, key) = state.is_a?(Hash) ? state[key] || state[key.to_sym] : nil

    # (A quest with no file may be at any stage.)
    def known_stage?(definition, stage) = stage.is_a?(String) && (definition.nil? || definition.stage?(stage))

    def restorable?(name, stage)
      definition = definition_of(name)
      return true if known_stage?(definition, stage)

      Debug.warn("Quests.replace: quest #{name} has no stage #{stage.inspect}: it is left out")
      false
    end
  end
end
