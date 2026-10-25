# Unit tests that fail, so that `redgem check --tests` has a failure to report.
class ScoreTest < RedGemTest::Case
  def test_a_coin_is_worth_ten
    assert_equal 10, 1
  end
end
