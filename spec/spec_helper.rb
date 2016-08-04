require "mruby_engine"

require "arbitrary"
require "property"

require "pathname"
require "rspec"

MEGABYTE = 1 << 20

module EngineSpecHelper
  MICRO_TEST_PATH = Pathname.new(__FILE__).dirname.join("micro_test.mrb")

  def reasonable_memory_quota
    4 * MEGABYTE
  end

  def reasonable_instruction_quota
    100_000
  end

  def reasonable_time_quota
    0.1r
  end

  def reasonable_engine
    MRubyEngine.new(
      reasonable_memory_quota,
      reasonable_instruction_quota,
      reasonable_time_quota,
    )
  end

  def make_test_engine
    engine = reasonable_engine
    engine.sandbox_eval("micro_test.rb", File.read(MICRO_TEST_PATH))
    engine
  end

  def squish(s)
    s = s.dup
    s.gsub!(/\A[[:space:]]+/, "")
    s.gsub!(/[[:space:]]+\z/, "")
    s.gsub!(/[[:space:]]+/, " ")
    s.freeze
  end
end
