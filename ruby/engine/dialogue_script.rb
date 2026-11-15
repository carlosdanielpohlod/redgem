# Dialogue::Script: a dialogue file as it was read (see dialogue.rb), and the rules its conditions
# and effects follow. The engine reads the file and refuses one that is not well made, so what is
# here is: every node named is there, and every condition and effect is one of those it knows.
module Dialogue
  class Script
    attr_reader :name

    # data: the file, read (string keys), as Dialogue.__read gives it.
    def initialize(name, data)
      @name = name
      @data = data
    end

    def start = @data["start"]

    # The node of that id (a Hash), or nil.
    def node(id) = @data["nodes"][id]

    def node?(id) = @data["nodes"].key?(id)

    def inspect = "#<Dialogue::Script #{name}>"
  end

  # What a condition reads, what it holds against, and what an effect does.
  module Rules
    COMPARISONS = {
      "is" => ->(value, wanted) { value == wanted },
      "not" => ->(value, wanted) { value != wanted },
      "at_least" => ->(value, wanted) { value.is_a?(Numeric) && value >= wanted },
      "at_most" => ->(value, wanted) { value.is_a?(Numeric) && value <= wanted }
    }.freeze
    EFFECTS = { "set" => :set, "add" => :add, "quest" => :advance, "event" => :event }.freeze

    class << self
      # Whether every condition of a list holds (an empty or missing list always does).
      def hold?(conditions) = (conditions || []).all? { |condition| holds?(condition) }

      # Where a "next" (a node's id, or branches) goes: a node's id, or nil (the end).
      def target(next_step)
        return next_step unless next_step.is_a?(Array)

        next_step.find { |branch| hold?(branch["if"]) }&.fetch("next")
      end

      # Makes a list of effects happen, for a conversation.
      def apply(effects, conversation)
        (effects || []).each { |effect| __send__(EFFECTS.fetch(kind_of(effect)), effect, conversation) }
      end

      private

      def holds?(condition)
        comparison = COMPARISONS.keys.find { |name| condition.key?(name) }
        COMPARISONS[comparison].call(value_of(condition), condition[comparison])
      end

      def value_of(condition)
        return Dialogue.quests.stage(condition["quest"]) if condition.key?("quest")

        GameVariables[condition["var"]]
      end

      def kind_of(effect) = EFFECTS.keys.find { |name| effect.key?(name) }

      def set(effect, _conversation)
        GameVariables[effect["set"]] = effect["to"]
      end

      # (A variable never set counts from 0.)
      def add(effect, _conversation)
        name = effect["add"]
        now = GameVariables[name]
        now = 0 if now.nil?
        raise ArgumentError, "dialogue: cannot add to #{name}, which holds #{now.inspect}" unless now.is_a?(Numeric)

        GameVariables[name] = now + effect["by"]
      end

      def advance(effect, conversation) = Dialogue.quests.set_stage(effect["quest"], effect["to"], conversation)

      def event(effect, conversation) = Dialogue.__event(effect["event"], effect["data"], conversation)
    end
  end
end
