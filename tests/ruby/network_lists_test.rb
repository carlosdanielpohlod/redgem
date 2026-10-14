# What another machine says in lists is first checked for being a list of the right length.
class NetworkListsTest < RedGemTest::Case
  Said = Network::Said

  def test_a_list_is_an_array_of_exactly_so_many_things
    assert Said.list?([1, 2, 3], 3)
    assert Said.list?([], 0)
    refute Said.list?([1, 2], 3)
    refute Said.list?([1, 2, 3, 4], 3)
  end

  def test_what_is_not_an_array_is_no_list
    refute Said.list?("abc", 3)
    refute Said.list?({ a: 1, b: 2, c: 3 }, 3)
    refute Said.list?(nil, 0)
  end
end
