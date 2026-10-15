# What a game's HTTP looks like from Ruby: requests that cannot go are refused at once, answers
# reach the block waiting for them, and JSON goes in and out. Real requests to real servers are
# tested in tests/integration/http_client_test.cpp; nothing here reaches beyond this machine.
class HttpTest < RedGemTest::Case
  NOBODY_THERE = "http://127.0.0.1:9/".freeze   # a port nothing listens on, on this machine
  OK = 200
  NOT_FOUND = 404
  NO_ANSWER = 0

  def setup = HTTP.__scene_reset

  def test_an_answer_reaches_the_block_that_waits_for_it
    got = nil
    id = HTTP.get(NOBODY_THERE) { |response| got = response }
    HTTP.__answer(id, OK, { "content-type" => "application/json" }, '{"top":[1,2]}', nil)
    assert got.ok?
    assert_equal OK, got.status
    assert_equal "application/json", got.headers["content-type"]
    assert_equal({ top: [1, 2] }, got.json)
  end

  def test_an_answer_is_not_ok_with_an_error_status_or_none_at_all
    refute HTTP::Response.new(NOT_FOUND, {}, "", nil).ok?
    refute HTTP::Response.new(NO_ANSWER, {}, "", "no answer within 15 seconds").ok?
  end

  def test_a_body_that_is_not_json_reads_as_nil
    assert_nil HTTP::Response.new(OK, {}, "<html>", nil).json
    assert_nil HTTP::Response.new(OK, {}, "#{"[" * 100}#{"]" * 100}", nil).json
  end

  def test_unknown_keys_come_as_strings_so_answers_make_no_symbols
    json = HTTP::Response.new(OK, {}, '{"zq_never_used_key":1}', nil).json
    assert_equal({ "zq_never_used_key" => 1 }, json)
  end

  def test_json_goes_out_as_json_with_its_content_type
    assert_equal '{"player":"ana","points":120}', HTTP.__json({ player: "ana", points: 120 })
    assert_raises(ArgumentError) { HTTP.post(NOBODY_THERE, json: { a: 1 }, body: "both") }
  end

  def test_a_request_that_cannot_go_is_refused_at_once_with_the_reason
    error = assert_raises(ArgumentError) { HTTP.get("ftp://example.com/") }
    assert error.message.include?("https://"), error.message
    error = assert_raises(ArgumentError) { HTTP.get(NOBODY_THERE, headers: { "X-Evil" => "a\r\nb" }) }
    assert error.message.include?("line break"), error.message
    assert_raises(ArgumentError) { HTTP.get(NOBODY_THERE, timeout: 0) }
  end

  def test_blocks_of_a_level_that_was_replaced_are_not_called
    called = false
    id = HTTP.delete(NOBODY_THERE) { called = true }
    HTTP.__scene_reset
    HTTP.__answer(id, OK, {}, "", nil)
    refute called
  end

  def test_each_request_has_a_number_of_its_own
    ids = [HTTP.put(NOBODY_THERE, body: "x"), HTTP.patch(NOBODY_THERE, json: [1])]
    assert_equal 2, ids.uniq.size
  end
end
