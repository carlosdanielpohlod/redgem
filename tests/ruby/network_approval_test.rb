# Connection approval as the prelude sees it, outside any game: the handler is asked, a decision
# is made once, identities are kept while their players are there, and a refused join says why.
# The machines themselves are tested in tests/integration/connection_approval_test.cpp.
class NetworkApprovalTest < RedGemTest::Case
  NEWCOMER = 2
  TICKET = 7
  NOBODY_THERE = "127.0.0.1".freeze
  SOME_PORT = 47_824

  def setup
    Network.authenticate
    Network.__scene_reset
  end

  def test_the_handler_gets_the_newcomer_what_it_carries_and_the_decision
    asked = nil
    Network.authenticate { |peer, auth, decide| asked = [peer, auth, decide] }
    Network.__event(:approval, NEWCOMER, "", { token: "abc" }, TICKET)
    assert_equal [NEWCOMER, { token: "abc" }], asked.first(2)
    assert asked.last.is_a?(Network::Decision)
  end

  def test_a_decision_is_made_once_and_needs_an_identity
    decision = Network::Decision.new(NEWCOMER, TICKET)
    assert_raises(ArgumentError) { decision.accept(identity: nil) }
    refute decision.deny("no")   # nobody waits for it here
    refute decision.accept(identity: "user-1")
  end

  def test_with_no_handler_the_newcomer_is_turned_away_quietly
    assert_equal false, Network.__event(:approval, NEWCOMER, "", {}, TICKET)
  end

  def test_who_a_player_is_is_known_until_after_the_game_heard_it_left
    seen = nil
    Network.on_leave { |peer| seen = [Network.identity(peer), Network.player_name(peer)] }
    Network.__identify(NEWCOMER, "user-4821", "Ana")
    assert_equal "user-4821", Network.identity(NEWCOMER)
    Network.__event(:left, NEWCOMER, "", nil)
    assert_equal %w[user-4821 Ana], seen
    assert_nil Network.identity(NEWCOMER)
  end

  def test_a_refused_join_says_why_and_a_failed_one_without_a_reason_says_nil
    reasons = []
    Network.on_fail { |reason| reasons << reason }
    Network.__event(:failed, 0, "not logged in", nil)
    Network.__event(:failed, 0, "", nil)
    assert_equal ["not logged in", nil], reasons
  end

  def test_what_a_join_carries_is_a_hash
    error = assert_raises(ArgumentError) { Network.join(NOBODY_THERE, port: SOME_PORT, auth: "token") }
    assert error.message.include?("auth:"), error.message
    assert Network.join(NOBODY_THERE, port: SOME_PORT, auth: { token: "abc" })
    Network.leave
  end
end
