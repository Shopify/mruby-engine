require "spec_helper"

RSpec.describe MRubyEngine do
  include EngineSpecHelper

  let(:engine) { make_test_engine }

  describe "#new" do
    it "raises if the memory quota is negative" do
      expect {
        MRubyEngine.new(-1, reasonable_instruction_quota, reasonable_time_quota)
      }.to raise_error(ArgumentError, "memory quota cannot be negative")
    end

    it "raises if the instruction quota is negative" do
      expect {
        MRubyEngine.new(reasonable_memory_quota, -1, reasonable_time_quota)
      }.to raise_error(ArgumentError, "instruction quota cannot be negative")
    end

    it "handles unaligned capacity" do
      expect {
        MRubyEngine.new(
          654321,
          reasonable_instruction_quota,
          reasonable_time_quota,
        )
      }.to_not raise_error
    end

    it "refuses to allocate ridiculously large amount of memory" do
      expect {
        MRubyEngine.new(
          1024 * MEGABYTE,
          reasonable_instruction_quota,
          reasonable_time_quota,
        )
      }.to raise_error(ArgumentError, squish(<<-MESSAGE))
        memory pool must be between 256KiB and 262144KiB (requested 1073741824B
        rounded to 1048576KiB)
      MESSAGE
    end

    it "refuses to allocate ludicrously small amount of memory" do
      expect {
        MRubyEngine.new(8, reasonable_instruction_quota, reasonable_time_quota)
      }.to raise_error(ArgumentError, squish(<<-MESSAGE))
        memory pool must be between 256KiB and 262144KiB (requested 8B rounded
        to 4KiB)
      MESSAGE
    end
  end

  describe "#stat" do
    it ":instructions is zero on a fresh engine" do
      stat = reasonable_engine.stat
      expect(stat[:instructions]).to eq(0)
    end

    it ":cpu_time is zero on a fresh engine" do
      stat = reasonable_engine.stat
      expect(stat[:cpu_time]).to eq(0)
    end

    it ":ctx_switches_v is nil on a fresh engine" do
      stat = reasonable_engine.stat
      expect(stat.key?(:ctx_switches_v)).to be true
      expect(stat[:ctx_switches_v]).to be nil
    end

    it ":ctx_switches_iv is nil on a fresh engine" do
      stat = reasonable_engine.stat
      expect(stat.key?(:ctx_switches_iv)).to be true
      expect(stat[:ctx_switches_iv]).to be nil
    end

    it ":instructions is non zero after executing some code" do
      engine.sandbox_eval("addition.rb", "1 + 1")
      expect(engine.stat[:instructions]).not_to eq(0)
    end

    it ":cpu_time is non zero after executing some code" do
      skip("Not supported on #{RUBY_PLATFORM}.") unless RUBY_PLATFORM =~ /linux/
      engine.sandbox_eval("addition.rb", "1 + 1")
      expect(engine.stat[:cpu_time]).not_to eq(0)
    end

    it ":ctx_switches_v is 0 or larger after executing some code" do
      skip("Not supported on #{RUBY_PLATFORM}.") unless RUBY_PLATFORM =~ /linux/
      engine.sandbox_eval("addition.rb", "1 + 1")
      expect(engine.stat[:ctx_switches_v]).to be >= 0
    end

    it ":ctx_switches_iv is 0 or larger after executing some code" do
      skip("Not supported on #{RUBY_PLATFORM}.") unless RUBY_PLATFORM =~ /linux/
      engine.sandbox_eval("addition.rb", "1 + 1")
      expect(engine.stat[:ctx_switches_iv]).to be >= 0
    end

    it ":instructions is equal to quota after reaching quota" do
      expect do
        engine.sandbox_eval("loop.rb", "loop { }")
      end.to raise_error(MRubyEngine::EngineInstructionQuotaError)
      expect(engine.stat[:instructions]).to eq(reasonable_instruction_quota)
    end

    it ":memory is within boundaries on a fresh engine" do
      expect(engine.stat[:memory]).to be > 0
      expect(engine.stat[:memory]).to be <= reasonable_memory_quota / 2
    end

    it ":memory is within boundaries on an engine that exceeded its memory quota" do
      begin
        engine.sandbox_eval("alloc_loop.rb", %(a = []; loop { a << ("foo" * 1000) }))
      rescue MRubyEngine::EngineMemoryQuotaError
        nil
      end
      expect(engine.stat[:memory]).to be > 0
      expect(engine.stat[:memory]).to be <= reasonable_memory_quota
    end
  end

  it "raises on a syntax error" do
    expect {
      engine.sandbox_eval("syntax_error.rb", "(")
    }.to raise_error(MRubyEngine::EngineSyntaxError, "syntax_error.rb:1:1: syntax error, unexpected $end")
  end

  it "raises if script raised an error" do
    expect {
      engine.sandbox_eval("raise.rb", %(raise("error!")))
    }.to raise_error(MRubyEngine::EngineRuntimeError, "error!")
  end

  it "saves guest backtrace when an error is raised" do
    expect {
      engine.sandbox_eval("backtrace.rb", <<-SOURCE)
        def foo
          raise StandardError, "foo"
        end

        def bar
          foo
        end

        bar
      SOURCE
    }.to raise_error(MRubyEngine::EngineRuntimeError) do |e|
      expect(e.guest_backtrace).to eq([
        "backtrace.rb:2:in Object.foo",
        "backtrace.rb:6:in Object.bar",
        "backtrace.rb:9",
      ])
    end
  end

  it "makes the exception type available" do
    expect {
      engine.sandbox_eval("error.rb", <<-SOURCE)
        class TransmogrificationError < StandardError; end
        raise(TransmogrificationError, "This looks bad.")
      SOURCE
    }.to raise_error(MRubyEngine::EngineRuntimeError) do |e|
      expect(e.type).to eq("TransmogrificationError")
    end
  end

  it "makes the exeption type available for nameless exceptions" do
    expect {
      engine.sandbox_eval("error.rb", <<-SOURCE)
        raise(Class.new(StandardError), "This looks bad.")
      SOURCE
    }.to raise_error(MRubyEngine::EngineRuntimeError) do |e|
      expect(e.type).to match(/\A#<Class:0x\h+>\z/)
    end
  end

  it "makes the exeption type available for exceptions with garbage to_s" do
    expect {
      engine.sandbox_eval("error.rb", <<-SOURCE)
        class TransmogrificationError < StandardError
          def self.to_s
            raise("dickbutt")
          end
        end
        raise(TransmogrificationError, "This looks bad.")
      SOURCE
    }.to raise_error(MRubyEngine::EngineRuntimeError) do |e|
      expect(e.type).to match(/\A#<Class:0x\h+>\z/)
    end
  end

  describe :inject do
    it "raises ArgumentError if not initialized" do
      expect do
        MRubyEngine.allocate.inject("@boom", "")
      end.to raise_error(ArgumentError, "uninitialized value when calling 'inject'")
    end

    it "makes a fixnum available inside the engine" do
      engine.inject("@foo", 17)
      engine.sandbox_eval("inject.rb", %(raise @foo.to_s unless @foo == 17))
    end

    it "makes a string available inside the engine" do
      engine.inject("@foo", "hello")
      engine.sandbox_eval("inject.rb", %(raise @foo.to_s unless @foo == "hello"))
    end

    it "makes a symbol available inside the engine" do
      engine.inject("@foo", :hello)
      engine.sandbox_eval("inject.rb", %(raise @foo.to_s unless @foo == :hello))
    end

    it "makes a hash available inside the engine" do
      engine.inject("@foo", foo: 17)
      engine.sandbox_eval("inject.rb", %(raise(@foo.to_s) unless @foo == {foo: 17}))
    end

    it "makes an array available inside the engine" do
      engine.inject("@foo", ["foo", 17])
      engine.sandbox_eval("inject.rb", %(assert_equal(["foo", 17], @foo)))
    end

    it "raise EngineQuotaAlreadyReached if quota was reached before" do
      engine = MRubyEngine.new(reasonable_memory_quota, 1, reasonable_time_quota)
      expect do
        engine.inject("@foo", ["foo", 17])
        engine.sandbox_eval("inject.rb", %(assert_equal(["foo", 17], @foo)))
      end.to raise_error(MRubyEngine::EngineInstructionQuotaError)

      expect {
        engine.inject("inject.rb", ["foo", 17])
      }.to raise_error(MRubyEngine::EngineQuotaAlreadyReached)
    end

    it "memory quota reached block next instruction inject" do
      mrb_engine = MRubyEngine.new(1 * MEGABYTE, reasonable_instruction_quota, reasonable_time_quota)
      expect do
        mrb_engine.sandbox_eval("alloc_loop.rb", %(a = []; loop { a << ("foo" * 1000) }))
      end.to raise_error(MRubyEngine::EngineMemoryQuotaError)

      expect {
        mrb_engine.inject("@bob", ["this"])
      }.to raise_error(MRubyEngine::EngineQuotaAlreadyReached)
    end
  end

  describe :extract do
    it "raises ArguementError if not initialized" do
      expect do
        MRubyEngine.allocate.extract("@boom")
      end.to raise_error(ArgumentError, "uninitialized value when calling 'extract'")
    end

    it "extracts a string" do
      engine.sandbox_eval("hello.rb", %(@hello = "hello"))
      expect(engine.extract("@hello")).to eq("hello")
    end

    it "extracts a symbol" do
      engine.sandbox_eval("hello.rb", "@hello = :hello")
      expect(engine.extract("@hello")).to eq(:hello)
    end

    it "extracts a fixnum" do
      engine.sandbox_eval("hello.rb", %(@hello = 42))
      expect(engine.extract("@hello")).to eq(42)
    end

    it "extracts a hash" do
      value = {
        "line_items" => [
          {
            "price" => {
              "__type__" => "money",
              "cents" => 500,
            },
            "quantity" => 5,
            "variant" => { "id" => 1 },
          },
          {
            "price" => {
              "__type__" => "money",
              "cents" => 800,
            },
            "quantity" => 1,
            "variant" => { "id" => 2 },
          },
        ],
      }
      engine.inject("@cart", value)
      expect(engine.extract("@cart")).to eq(value)
    end

    it "extracts a hash with symbolic keys" do
      engine.sandbox_eval("hash.rb", %(@my_hash = {foo: 1}))
      expect(engine.extract("@my_hash")).to eq(foo: 1)
    end

    it "extracts an array" do
      engine.sandbox_eval("extract.rb", %(@foo = ["foo", 17]))
      expect(engine.extract("@foo")).to eq(["foo", 17])
    end

    it "gracefully fails on random type" do
      engine.sandbox_eval("extract.rb", "@foo = Class.new")
      expect do
        engine.extract("@foo")
      end.to raise_error(MRubyEngine::EngineTypeError, squish(<<-MESSAGE))
        can only extract strings, fixnums, symbols, arrays or hashes
      MESSAGE
    end

    it "raise EngineQuotaAlreadyReached if quota was reached before" do
      engine = MRubyEngine.new(reasonable_memory_quota, 1, reasonable_time_quota)
      expect do
        engine.inject("@foo", ["foo", 17])
        engine.sandbox_eval("inject.rb", %(assert_equal(["foo", 17], @foo)))
      end.to raise_error(MRubyEngine::EngineInstructionQuotaError)
      expect {
        engine.extract("@foo")
      }.to raise_error(MRubyEngine::EngineQuotaAlreadyReached)
    end

    it "memory quota reached block next instruction extract" do
      mrb_engine = MRubyEngine.new(1 * MEGABYTE, reasonable_instruction_quota, reasonable_time_quota)
      expect do
        mrb_engine.inject("@bob", ["this"])
        mrb_engine.sandbox_eval("alloc_loop.rb", %(a = []; loop { a << ("foo" * 1000) }))
      end.to raise_error(MRubyEngine::EngineMemoryQuotaError)

      expect {
        mrb_engine.extract("@bob")
      }.to raise_error(MRubyEngine::EngineQuotaAlreadyReached)
    end
  end

  it "handes large integers" do
    value = 1_218_120_389
    engine.inject("@value", value)
    expect(engine.extract("@value")).to eq(value)
  end

  it "handles multi-byte characters" do
    engine.inject("@unicode", "🌈")
    engine.sandbox_eval("unicode.rb", <<-SOURCE)
      assert_equal(1, @unicode.length)
      @unicode += @unicode[0]
    SOURCE
    expect(engine.extract("@unicode")).to eq("🌈🌈")
  end

  describe :sandbox_eval do
    it "raise EngineQuotaAlreadyReached if quota was reached before" do
      engine = MRubyEngine.new(reasonable_memory_quota, 1, reasonable_time_quota)
      expect do
        engine.inject("@foo", ["foo", 17])
        engine.sandbox_eval("inject.rb", %(assert_equal(["foo", 17], @foo)))
      end.to raise_error(MRubyEngine::EngineInstructionQuotaError)
      expect do
        engine.sandbox_eval("inject.rb", "loop {@a = @b}")
      end.to raise_error(MRubyEngine::EngineQuotaAlreadyReached)
    end

    it "raises ArgumentError if not initialized" do
      expect {
        MRubyEngine.allocate.sandbox_eval("(boom)", "")
      }.to raise_error(ArgumentError, "uninitialized value when calling 'sandbox_eval'")
    end

    it "raises EngineMemoryQuotaError if it exceeds memory limits" do
      expect do
        engine.sandbox_eval("(out_of_memory)", <<-SOURCE)
          accumulator = []
          loop do
            accumulator << "I will run out of memory." * 20
          end
        SOURCE
      end.to raise_error(
        MRubyEngine::EngineMemoryQuotaError,
        /failed to allocate \d+ bytes \(\d+ bytes out of \d+ in use\)/,
      )
    end

    it "raises an EngineInstructionQuotaError when the instruction quota is exceeded" do
      expect do
        engine.sandbox_eval("loop.rb", "loop do; end")
      end.to raise_error(MRubyEngine::EngineInstructionQuotaError, "exceeded quota of 100000 instructions.")
    end

    it "raises an EngineTimeQuotaError when it runs for too long" do
      skip("Not supported on #{RUBY_PLATFORM}.") unless RUBY_PLATFORM =~ /linux/
      expect do
        engine.sandbox_eval("loop.rb", <<-SOURCE)
          a = "a" * 800000
          b = "a" * 400000
          b[-1] = "b"
          a.include?(b)
        SOURCE
      end.to raise_error(MRubyEngine::EngineTimeQuotaError, "exceeded quota of 100 ms.")
    end

    it "raises an EngineStackExhaustedError when the stack is about to overflow" do
      skip("Not supported on #{RUBY_PLATFORM}.") unless RUBY_PLATFORM =~ /linux/
      expect do
        engine.sandbox_eval("recursive_initialize.rb", <<-SOURCE)
          class A
            def initialize
              A.new
            end
          end
          A.new
        SOURCE
      end.to raise_error(MRubyEngine::EngineStackExhaustedError, "stack exhausted")
    end

    it "memory quota reached block next instruction eval " do
      memory_quota = 1 * MEGABYTE
      mrb_engine = MRubyEngine.new(memory_quota, reasonable_instruction_quota, reasonable_time_quota)
      expect {
        mrb_engine.sandbox_eval("alloc_loop.rb", %(a = []; loop { a << ("foo" * 1000) }))
      }.to raise_error(
        MRubyEngine::EngineMemoryQuotaError,
        /failed to allocate \d+ bytes \(\d+ bytes out of #{memory_quota} in use\)/,
      )

      expect {
        mrb_engine.sandbox_eval("alloc_loop.rb", %(a = []; loop { a << ("foo" * 1000) }))
      }.to raise_error(MRubyEngine::EngineQuotaAlreadyReached)
    end
  end

  describe :load_instruction_sequence do
    it "runs an instruction sequence" do
      iseq = MRubyEngine::InstructionSequence.new([["sample.rb", "@foo = [42]"]])
      engine.load_instruction_sequence(iseq)
      expect(engine.extract("@foo")).to eq([42])
    end

    it "runs an instruction sequence containing multiple files" do
      iseq = MRubyEngine::InstructionSequence.new(
        [
          ["sample.rb", %(@foo = [42])],
          ["other.rb", %(@bar = ["banana"])],
        ],
      )
      engine.load_instruction_sequence(iseq)
      expect(engine.extract("@bar")).to eq(["banana"])
    end

    it "load_instruction_sequence should exced quota of instruction and fail excution" do
      iseq = MRubyEngine::InstructionSequence.new(
        [
          ["other.rb", " loop do; end "],
        ],
      )
      expect {
        engine.load_instruction_sequence(iseq)
      }.to raise_error(MRubyEngine::EngineInstructionQuotaError)
      expect(engine.stat[:instructions]).to eq(reasonable_instruction_quota)
    end

    it "load_instruction_sequence should block next instruction if quota was reached from instruction quota" do
      iseq = MRubyEngine::InstructionSequence.new([["other.rb", " loop do; end "]])
      expect {
        engine.load_instruction_sequence(iseq)
      }.to raise_error(MRubyEngine::EngineInstructionQuotaError)

      expect do
        iseq = MRubyEngine::InstructionSequence.new([["other.rb", "@bob = 1"]])
        engine.load_instruction_sequence(iseq)
      end.to raise_error(MRubyEngine::EngineQuotaAlreadyReached)
    end

    it "load_instruction_sequence should block next instruction if quota was reached from memory error quota" do
      iseq = MRubyEngine::InstructionSequence.new([["other.rb", %(a = []; loop { a << ("foo" * 1000) })]])
      expect {
        engine.load_instruction_sequence(iseq)
      }.to raise_error(MRubyEngine::EngineMemoryQuotaError)

      expect do
        iseq = MRubyEngine::InstructionSequence.new([["other.rb", "@bob = 1"]])
        engine.load_instruction_sequence(iseq)
      end.to raise_error(MRubyEngine::EngineQuotaAlreadyReached)
    end
  end

  describe MRubyEngine::InstructionSequence do
    describe :new do
      it "raises when no source is provided" do
        expect do
          MRubyEngine::InstructionSequence.new([])
        end.to raise_error(ArgumentError, "can't create empty instruction sequence")
      end
    end

    describe :size do
      it "yields to size of the compiled instructions" do
        iseq = MRubyEngine::InstructionSequence.new([["sample.rb", "@foo = 42"]])
        expect(iseq.size).to be > 0
      end
    end

    describe :hash do
      it "returns the same hash for the same sequence" do
        source = [["hello.rb", %(loop { puts("hello") })]]
        expect(MRubyEngine::InstructionSequence.new(source).hash).to eq(
          MRubyEngine::InstructionSequence.new(source).hash,
        )
      end
    end

    it "reports syntax errors" do
      expect do
        MRubyEngine::InstructionSequence.new([["sample.rb", "("]])
      end.to raise_error(MRubyEngine::EngineSyntaxError, "sample.rb:1:1: syntax error, unexpected $end")
    end

    it "reports syntax error with multiple files" do
      expect do
        MRubyEngine::InstructionSequence.new(
          [["sample.rb", "a = 1"], ["sample_2.rb", "("]],
        )
      end.to raise_error(MRubyEngine::EngineSyntaxError, "sample_2.rb:1:1: syntax error, unexpected $end")
    end
  end
end
