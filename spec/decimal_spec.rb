require "arbitrary"
require "bigdecimal"
require "bigdecimal/util"
require "mruby_engine"
require "property"
require "spec_helper"

describe "Decimal" do
  include EngineSpecHelper
  include PropertyTesting
  using Arbitrary

  def reasonable_memory_quota
    8 * MEGABYTE
  end

  let(:engine) { make_test_engine }

  def eval_test(source)
    engine.sandbox_eval("test.rb", source)
  end

  describe :new do
    it "raises if passed nil" do
      eval_test(<<-SOURCE)
        assert_raises(ArgumentError, "can't convert nil into Decimal") do
          Decimal.new(nil)
        end
      SOURCE
    end

    it "raises if passed invalid string" do
      eval_test(<<-SOURCE)
        assert_raises(ArgumentError, %(can't convert "lol" into Decimal)) do
          Decimal.new("lol")
        end
      SOURCE
    end

    it "raises if passed an object not responding to :to_d" do
      eval_test(<<-SOURCE)
        assert_raises(ArgumentError, "can't convert #<Object:0x0> into Decimal") do
          Decimal.new(Object.new)
        end
      SOURCE
    end

    it "doesn't fail assertion if re-initialized with self" do
      eval_test(<<-RUBY)
        a = Decimal.new(1.5)
        a.initialize a
        assert_equal "1.5", a.to_s
      RUBY
    end
  end

  describe :to_s do
    it "formats zero as '0'" do
      eval_test(<<-SOURCE)
        assert_equal("0", Decimal.new(0).to_s)
      SOURCE
    end

    it "formats one as '1'" do
      eval_test(<<-SOURCE)
        assert_equal("1", Decimal.new(1).to_s)
      SOURCE
    end

    it "formats minus 5 as '-5'" do
      eval_test(<<-SOURCE)
        assert_equal("-5", Decimal.new(-5).to_s)
      SOURCE
    end

    it "formats '-100.4'" do
      eval_test(<<-SOURCE)
        assert_equal("-100.4", Decimal.new("-100.4").to_s)
      SOURCE
    end

    it "will be reparsed as the same number" do
      property(BigDecimal.arbitrary) do |decimal|
        engine.inject("@input", "decimal" => decimal.to_s)
        eval_test(<<-SOURCE)
          decimal = @input["decimal"].to_d
          assert_equal(decimal, decimal.to_s.to_d)
        SOURCE
      end
    end

    it "works if Decimal class is replaced" do
      eval_test(<<-SOURCE)
        begin
          a = Decimal.new(1)
          old_decimal = Decimal
          Decimal = Hash
          assert_equal("-1", (-a).to_s)
          assert_equal("2", (a+a).to_s)
        ensure
          Decimal = old_decimal
        end
      SOURCE
    end
  end

  describe :+ do
    it "can add a fixnum" do
      eval_test(<<-SOURCE)
        assert_equal(Decimal.new(5), Decimal.new(1) + 4)
      SOURCE
    end

    it "can add another decimal" do
      eval_test(<<-SOURCE)
        assert_equal(Decimal.new("5.72"), Decimal.new(1) + Decimal.new("4.72"))
      SOURCE
    end

    it "raises if right operand does not implement :to_d" do
      eval_test(<<-SOURCE)
        assert_raises(TypeError, "can't convert Object into Decimal") do
          Decimal.new + Object.new
        end
      SOURCE
    end

    it "is identical to MRI's implementation" do
      property(BigDecimal.arbitrary, BigDecimal.arbitrary) do |left, right|
        engine.inject("@decimals", [left.to_s, right.to_s])
        eval_test("@result = [(@decimals[0].to_d + @decimals[1].to_d).to_s]")
        expect(engine.extract("@result")[0].to_d).to eq(left + right)
      end
    end
  end

  describe :- do
    it "can substract two decimals" do
      eval_test(<<-SOURCE)
        assert_equal("-6808.8822".to_d, "27314.652".to_d - "34123.5342".to_d)
      SOURCE
    end

    it "is identical to MRI's implementation" do
      property(BigDecimal.arbitrary, BigDecimal.arbitrary) do |left, right|
        engine.inject("@decimals", [left.to_s, right.to_s])
        eval_test("@result = [(@decimals[0].to_d - @decimals[1].to_d).to_s]")
        expect(engine.extract("@result")[0].to_d).to eq(left - right)
      end
    end
  end

  describe :-@ do
    it "negates zero" do
      eval_test(<<-SOURCE)
        assert_equal(0.to_d, -0.to_d)
      SOURCE
    end

    it "negates integer" do
      eval_test(<<-SOURCE)
        assert_equal("-123".to_d, -"123".to_d)
      SOURCE
    end

    it "negates rational" do
      eval_test(<<-SOURCE)
        assert_equal("-123.9835".to_d, -"123.9835".to_d)
      SOURCE
    end

    it "is identical to MRI's implementation" do
      property(BigDecimal.arbitrary) do |decimal|
        engine.inject("@decimal", [decimal.to_s])
        eval_test("@decimal = [(-@decimal[0].to_d).to_s]")
        expect(engine.extract("@decimal")[0].to_d).to eq(-decimal)
      end
    end
  end

  describe :* do
    it "can multiply two numbers" do
      eval_test(<<-SOURCE)
        assert_equal("166.8".to_d, "1.5".to_d * "111.2")
      SOURCE
    end

    it "is identical to MRI's implementation" do
      property(BigDecimal.arbitrary, BigDecimal.arbitrary) do |left, right|
        engine.inject("@decimals", [left.to_s, right.to_s])
        eval_test("@result = [(@decimals[0].to_d * @decimals[1].to_d).to_s]")
        expect(engine.extract("@result")[0].to_d).to eq(left * right)
      end
    end
  end

  describe :/ do
    it "automatically rounds according to precision" do
      eval_test(<<-SOURCE)
        assert_equal(("0." + "3" * Decimal::PRECISION).to_d, 1.to_d / 3)
      SOURCE
    end
  end

  describe :<=> do
    it "is 0 for two zeroes" do
      eval_test(<<-SOURCE)
        assert_equal(0, 0.to_d <=> 0.to_d)
      SOURCE
    end

    it "is 0 for a decimal zero and a fixnum zero" do
      eval_test(<<-SOURCE)
        assert_equal(0, 0.to_d <=> 0)
      SOURCE
    end

    it "is 0 for non-integer value and its string representation" do
      eval_test(<<-SOURCE)
        assert_equal(0, "5.2".to_d <=> "5.2")
      SOURCE
    end

    it "yields the same result MRI's implementation" do
      property(BigDecimal.arbitrary, BigDecimal.arbitrary) do |left, right|
        engine.inject("@decimals", [left.to_s, right.to_s])
        eval_test("@result = [@decimals[0].to_d <=> @decimals[1].to_d]")
        expect(engine.extract("@result")[0]).to eq(left <=> right)
      end
    end
  end

  describe :eql? do
    it "is true for equal fixnums" do
      eval_test(<<-SOURCE)
        assert_eql(0.to_d, 0.to_d)
        assert_eql(1.to_d, 1.to_d)
        assert_eql((-42).to_d, (-42).to_d)
        assert_eql("6.28319".to_d, "6.28319".to_d)
      SOURCE
    end

    it "is true for equal fixnums (property)" do
      property(BigDecimal.arbitrary) do |decimal|
        engine.inject("@input", "decimal" => decimal.to_s)
        eval_test(<<-SOURCE)
          assert_eql(@input["decimal"].to_d, @input["decimal"].to_d)
        SOURCE
      end
    end

    it "is false for to different fixnums" do
      eval_test(<<-SOURCE)
        refute_eql(0.to_d, 1.to_d)
        refute_eql(1.to_d, "-1".to_d)
        refute_eql((-42).to_d, (-41).to_d)
        refute_eql("6.28319".to_d, "6.38319".to_d)
      SOURCE
    end

    it "is false for different fixnums (property)" do
      property(BigDecimal.arbitrary, BigDecimal.arbitrary) do |left, right|
        next if left == right
        engine.inject("@input", "left" => left.to_s, "right" => right.to_s)
        eval_test(<<-SOURCE)
          refute_eql(@input["left"].to_d, @input["right"].to_d)
        SOURCE
      end
    end

    it "is always false with non-decimal" do
      eval_test(<<-SOURCE)
        refute_eql(0.to_d, 0)
        refute_eql(1.to_d, "1")
        refute_eql((-42).to_d, -42)
        refute_eql("6.28319".to_d, 6.28319)
      SOURCE
    end
  end

  describe :hash do
    it "returns the same value for identical decimal numbers" do
      eval_test(<<-SOURCE)
        assert_equal(0.to_d.hash, 0.to_d.hash)
        assert_equal(1.to_d.hash, 1.to_d.hash)
        assert_equal("3.141592".to_d.hash, "3.141592".to_d.hash)
        assert_equal("-2.718281828459045".to_d.hash, "-2.718281828459045".to_d.hash)
      SOURCE
    end

    it "returns the different values for different decimal numbers" do
      eval_test(<<-SOURCE)
        refute_equal(0.to_d.hash, 1.to_d.hash)
        refute_equal(1.to_d.hash, 10000000001.to_d.hash)
        refute_equal("3.141592".to_d.hash, "3.1415921".to_d.hash)
        refute_equal("-2.718281828459045".to_d.hash, "-2.718281828459044".to_d.hash)
      SOURCE
    end
  end

  describe :round do
    it "rounds to nearest integer" do
      eval_test(<<-SOURCE)
        assert_equal("5".to_d, "5.45".to_d.round)
        assert_equal("6".to_d, "5.5".to_d.round)
        assert_equal("0".to_d, "-0.45".to_d.round)
        assert_equal("-1".to_d, "-0.5".to_d.round)
      SOURCE
    end

    it "yields the same result MRI's implementation" do
      property(BigDecimal.arbitrary) do |decimal|
        engine.inject("@input", "decimal" => decimal.to_s)
        eval_test(%q(@output = {"actual" => @input["decimal"].to_d.round.to_s}))
        expect(engine.extract("@output")["actual"].to_d).to eq(decimal.round)
      end
    end
  end

  describe :floor do
    it "rounds towards negative infinity" do
      eval_test(<<-SOURCE)
        assert_equal("5".to_d, "5.45".to_d.floor)
        assert_equal("5".to_d, "5.5".to_d.floor)
        assert_equal("-1".to_d, "-0.45".to_d.floor)
        assert_equal("-1".to_d, "-0.5".to_d.floor)
      SOURCE
    end

    it "yields the same result MRI's implementation" do
      property(BigDecimal.arbitrary) do |decimal|
        engine.inject("@input", "decimal" => decimal.to_s)
        eval_test(%q(@output = {"actual" => @input["decimal"].to_d.floor.to_s}))
        expect(engine.extract("@output")["actual"].to_d).to eq(decimal.floor)
      end
    end
  end

  describe :ceil do
    it "rounds towards positive infinity" do
      eval_test(<<-SOURCE)
        assert_equal("6".to_d, "5.45".to_d.ceil)
        assert_equal("6".to_d, "5.5".to_d.ceil)
        assert_equal("0".to_d, "-0.45".to_d.ceil)
        assert_equal("0".to_d, "-0.5".to_d.ceil)
      SOURCE
    end

    it "yields the same result MRI's implementation" do
      property(BigDecimal.arbitrary) do |decimal|
        engine.inject("@input", "decimal" => decimal.to_s)
        eval_test(%q(@output = {"actual" => @input["decimal"].to_d.ceil.to_s}))
        expect(engine.extract("@output")["actual"].to_d).to eq(decimal.ceil)
      end
    end
  end
end
