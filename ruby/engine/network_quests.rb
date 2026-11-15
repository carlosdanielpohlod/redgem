# Quests over the network (see quests.rb). Their state is GameVariables, which the host owns and
# tells every machine of; what a client's player collects, delivers, defeats or reaches is told to
# the host here, which counts it as its own player's.
module Network
  class << self
    private

    def receive_quest_report(data, from)
      Quests.__report(*data) if host? && from != HOST && Said.quest_report?(data)
    end
  end

  module Said
    QUEST_REPORT_SIZE = 4

    class << self
      # [kind, subject, other or nil, count]: what a player did, as Quests says it.
      def quest_report?(data)
        list?(data, QUEST_REPORT_SIZE) && Quests::REPORTED.include?(data[0]) && report_parts?(*data[1..])
      end

      private

      def report_parts?(subject, other, count)
        object_key?(subject) && no_or_key?(other) && report_count?(count)
      end

      def report_count?(count) = count.is_a?(Integer) && count.between?(1, Quests::MAX_COUNT)
    end
  end
end
