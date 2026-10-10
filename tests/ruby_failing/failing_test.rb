# Deliberately failing test: ctest expects this run to fail (WILL_FAIL).
class HarnessFailureTest < RedGemTest::Case
  def test_failure_is_reported
    assert_equal 1, 2
  end
end
