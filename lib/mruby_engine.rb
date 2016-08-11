require "mruby_engine/mruby_engine"
require "mruby_engine/version"

class MRubyEngine
  class EngineRuntimeError < EngineError
    attr_accessor :guest_backtrace, :type
  end

  class InstructionSequence
    def hash
      @hash ||= compute_hash
    end
  end
end
