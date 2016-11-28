require "spec_helper"
require "mruby_engine"

RSpec.describe "Kernel" do
  include EngineSpecHelper

  let(:engine) { make_test_engine }

  def eval_test(source)
    engine.sandbox_eval("exit.rb", source)
  end

  describe :exit do
    it "stops execution and returns" do
      eval_test(<<-SOURCE)
        @bob  = "bob"
        exit
        @bob = "alex"
      SOURCE
      expect(engine.extract("@bob")).to eq("bob")
    end
  end

  describe :raise_error do
    it "does not raise an error when using the exit method" do
      expect {
        eval_test("exit")
      }.to_not raise_error
    end
  end

  describe :exit_when_exit_exception_removed do
    it "does not raise an error when ExitException is re-assigned" do
      expect {
        eval_test(<<-RUBY)
          Object.const_set('ExitException', 1)
          exit
        RUBY
      }.to_not raise_error
    end
  end
end
