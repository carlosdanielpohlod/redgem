# What the host tells the others about the objects spawned so far, and how often it says it again.
module Network
  # How often the host tells one machine again what is spawned. Each machine has a few answers
  # saved up, for levels loaded one right after the other, and earns another every few seconds:
  # whoever asks more often than that is not answered.
  module Resends
    SAVED_UP = 4           # answers a machine can have waiting for it
    SECONDS_FOR_ONE = 2.0  # of game time, to earn one more

    @accounts = {}   # peer => [answers left, when that was counted]

    class << self
      # Whether `peer`, asking at `now` (seconds of game time), gets an answer; one is spent if so.
      def answer?(peer, now)
        left, counted_at = @accounts[peer] || [SAVED_UP, now]
        left = [left + (now - counted_at) / SECONDS_FOR_ONE, SAVED_UP].min
        answered = left >= 1
        @accounts[peer] = [answered ? left - 1 : left, now]
        answered
      end

      # Whoever comes next under that id starts with all its answers.
      def forget(peer) = @accounts.delete(peer)
    end
  end
end

module Network
  # What the host tells the others about the objects spawned so far.
  class << self
    private

    # (Asking is one short message and the answer is every object there is: a machine is only
    # answered as often as loading levels takes, see Resends.)
    def receive_resend(_nothing, from)
      send_spawned_to(from) if answers?(from)
    end

    # Whether this machine is the one to answer, and `peer` has an answer left (which is then spent).
    def answers?(peer) = host? && Resends.answer?(peer, Time.elapsed)

    def greet(peer)
      Resends.forget(peer)
      send_spawned_to(peer)
    end

    # The host tells a machine (a newcomer, or one that asked again) what is spawned.
    def send_spawned_to(peer)
      __deliver(SPAWNED, spawned_records, peer) if host?
    end

    # What a newcomer needs to see the objects spawned before it arrived, where they are now.
    def spawned_records
      @spawned.to_a.select { |_key, entry| entry[:object].alive? }.map { |key, entry| record_of(key, entry) }
    end
  end
end
