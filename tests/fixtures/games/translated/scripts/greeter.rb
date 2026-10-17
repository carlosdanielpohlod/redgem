# Greets in the language in use (the fixture's scripts folder: its translations are in ../locales).
class Greeter < Behaviour
  def self.greeting(name) = I18n.t("greeting", name: name)
end
