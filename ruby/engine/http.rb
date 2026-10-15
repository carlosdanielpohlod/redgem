# The web, from a game: requests to web services and to your own backend. The frame never waits:
# the block gets the answer a moment later, on the game's own thread.
#
#   HTTP.get("https://api.example.com/news") { |response| @news = response.json if response.ok? }
#   HTTP.post("https://api.example.com/scores", json: { points: 120 },
#             headers: { "Authorization" => "Bearer #{token}" }) do |response|
#     Debug.warn("scores: #{response.error || response.status}") unless response.ok?
#   end
#
# Addresses are https:// (the server's certificate is checked against the system's), or http://
# to this machine or the local network. An answer is at most 8 MB, 16 requests may be on their
# way at once, and each gives up after 15 seconds unless timeout: says otherwise (at most 120).
# Redirects are not followed. When a level is loaded, the blocks of requests still on their way
# are not called (the requests still go out); when the game ends, those requests are given up.
module HTTP
  DEFAULT_TIMEOUT = 15.0   # seconds
  SUCCESS = (200..299)     # the statuses ok? stands for
  CONTENT_TYPE = "Content-Type".freeze
  JSON_TYPE = "application/json".freeze

  # What came back. With no answer at all (no connection, a timeout, an untrusted certificate)
  # status is 0 and error says why.
  class Response
    attr_reader :status, :headers, :body, :error

    def initialize(status, headers, body, error)
      @status = status
      @headers = headers   # names in lower case: response.headers["content-type"]
      @body = body
      @error = error
    end

    # The server answered, with a 2xx status.
    def ok? = error.nil? && SUCCESS.cover?(status)

    # The body read as JSON; nil when it is not JSON. Hash keys come as symbols when the game's
    # scripts use that symbol (response.json[:name]), and as strings otherwise.
    def json = @json ||= HTTP.__parse_json(body)
  end

  @waiting = {}

  class << self
    def get(url, headers: {}, timeout: DEFAULT_TIMEOUT, &)
      request(:get, url, headers: headers, timeout: timeout, &)
    end

    def delete(url, headers: {}, timeout: DEFAULT_TIMEOUT, &)
      request(:delete, url, headers: headers, timeout: timeout, &)
    end

    # json: sends a value as JSON (a hash, an array...); body: sends a text as it is.
    def post(url, json: nil, body: nil, headers: {}, timeout: DEFAULT_TIMEOUT, &)
      request(:post, url, json: json, body: body, headers: headers, timeout: timeout, &)
    end

    def put(url, json: nil, body: nil, headers: {}, timeout: DEFAULT_TIMEOUT, &)
      request(:put, url, json: json, body: body, headers: headers, timeout: timeout, &)
    end

    def patch(url, json: nil, body: nil, headers: {}, timeout: DEFAULT_TIMEOUT, &)
      request(:patch, url, json: json, body: body, headers: headers, timeout: timeout, &)
    end

    # Any of them by its method's name. The request's number (what is waiting for it).
    def request(verb, url, json: nil, body: nil, headers: {}, timeout: DEFAULT_TIMEOUT, &block)
      raise ArgumentError, "HTTP: give json: or body:, not both" unless json.nil? || body.nil?

      headers = texts(headers)
      body = encode(json, headers) unless json.nil?
      id = __start(verb.to_s.upcase, url.to_s, headers, body.to_s, timeout.to_f)
      @waiting[id] = block if block
      id
    end

    # Called by the engine with the answer to request `id`.
    def __answer(id, status, headers, body, error)
      @waiting.delete(id)&.call(Response.new(status, headers, body, error))
    end

    # Called by the engine when a level is loaded: the blocks waiting belonged to the old one.
    def __scene_reset = @waiting = {}

    private

    def texts(headers) = headers.each_with_object({}) { |(name, value), all| all[name.to_s] = value.to_s }

    def encode(json, headers)
      headers[CONTENT_TYPE] = JSON_TYPE unless headers.keys.any? { |name| name.downcase == CONTENT_TYPE.downcase }
      __json(json)
    end
  end
end
