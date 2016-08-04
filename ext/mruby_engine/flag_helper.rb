module Flags
  class << self
    def cflags
      %w(-pthread) + debug_flags + optimization_flags
    end

    def debug_flags
      %w(-g3)
    end

    def optimization_flags
      if ENV['MRUBY_ENGINE_ENABLE_DEBUG']
        %w(-O0)
      else
        %w(-O3)
      end
    end

    def io_safe_defines
      defines = %w(
        MRB_ENABLE_DEBUG_HOOK
        MRB_INT64
        MRB_UTF8_STRING
        MRB_WORD_BOXING
        YYDEBUG
      )
      defines << "_GNU_SOURCE" if RUBY_PLATFORM =~ /linux/
      defines
    end

    def defines
      io_safe_defines + %w(MRB_DISABLE_STDIO UNW_LOCAL_ONLY)
    end
  end
end
