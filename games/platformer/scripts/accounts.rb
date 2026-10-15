# Who plays online. With a backend (ACCOUNTS in MainScene, such as the example backend of the
# manual's Online services page), a machine logs in to it and joins the dedicated server with the
# token it got; without one, it joins as a guest, under an id it keeps in Save. The server's side
# is ServerGate. The login lasts until the game is closed.
module Accounts
  GUEST_KEY = :guest_id
  GUEST_ID_DIGITS = 16
  HEX = "0123456789abcdef".freeze
  LOGIN_FAILED = "the login failed".freeze

  class << self
    attr_reader :name

    def backend = MainScene::ACCOUNTS
    def backend? = !backend.empty?
    def logged_in? = !@token.nil?

    # What joining the server carries: the login's token, or this machine's guest id.
    def auth = logged_in? ? { token: @token } : { guest: guest_id }

    # Logs in to the backend; the block gets nil once logged in, or why not.
    def log_in(name, password, &done)
      HTTP.post("#{backend}/login", json: { name: name, password: password }) do |response|
        done&.call(take_answer(response))
      end
    end

    def guest_id = (Save[GUEST_KEY] ||= new_guest_id)

    private

    # Keeps the token the backend gave; nil once logged in, or why not.
    def take_answer(response)
      answer = response.json || {}
      @token = response.ok? ? answer[:token] : nil
      @name = answer[:name]
      why_not(answer, response) unless logged_in?
    end

    def why_not(answer, response) = answer[:error] || response.error || LOGIN_FAILED

    def new_guest_id
      random = Random.new(Time.now.usec)
      Array.new(GUEST_ID_DIGITS) { HEX[random.rand(HEX.size)] }.join
    end
  end
end
