require "time"
require "mruby_engine"
require "spec_helper"

describe "Time" do
  include EngineSpecHelper

  let(:engine) { make_test_engine }

  def eval_test(source)
    engine.sandbox_eval("test.rb", source)
  end

  it "test the functionality of time" do
    test = Pathname.new(__FILE__).dirname.join("mruby_time.mrb")
    engine.sandbox_eval("mruby_time.rb", File.read(test))
  end

  describe "#new" do
    it "returns a fixed moment in time" do
      eval_test(<<-SOURCE)
        assert_equal(Time.new(1970, 1, 1, 0, 0, 0, 0), Time.new)
      SOURCE
    end
  end

  it "does not respond to Time.now" do
    eval_test(<<-SOURCE)
      refute_respond_to(Time, :now)
    SOURCE
  end

  it "does not respond to Time.local" do
    eval_test(<<-SOURCE)
      refute_respond_to(Time, :local)
    SOURCE
  end

  it "does not respond to Time.localtime" do
    eval_test(<<-SOURCE)
      refute_respond_to(Time, :localtime)
    SOURCE
  end

  it "does not respond to Time.mktime" do
    eval_test(<<-SOURCE)
      refute_respond_to(Time, :mktime)
    SOURCE
  end

  it "does not respond to Time.gmt?" do
    eval_test(<<-SOURCE)
     refute_respond_to(Time, :gmt?)
    SOURCE
  end

  it "does not respond to Time.getgm" do
    eval_test(<<-SOURCE)
      refute_respond_to(Time, :getgm)
    SOURCE
  end

  it "does not respond to Time.gmtime" do
    eval_test(<<-SOURCE)
      refute_respond_to(Time, :gmtime)
    SOURCE
  end

  it "does not respond to Time.gm" do
    eval_test(<<-SOURCE)
      refute_respond_to(Time, :gm)
    SOURCE
  end

  it "does not respond to Time.gm" do
    eval_test(<<-SOURCE)
      refute_respond_to(Time, :getlocal)
    SOURCE
  end

  it "raises if time is out of range" do
    eval_test(<<-SOURCE)
      assert_raises(ArgumentError, "9.3674872249306e+17 out of Time range") do
        Time.at(0xd00000000000000)
      end
    SOURCE
  end
end
