require "bigdecimal"
require "bigdecimal/util"
require "securerandom"

module Arbitrary
  refine Fixnum.singleton_class do
    def arbitrary
      lambda do
        if SecureRandom.random_number(2) == 0
          arbitrary_natural.call
        else
          -arbitrary_natural.call
        end
      end
    end

    def arbitrary_natural
      lambda do
        SecureRandom.random_number(1_000_000_000)
      end
    end
  end

  refine Fixnum do
    def shrink
      if self > 2 || -2 < self
        [(self / 2).to_i]
      else
        []
      end
    end
  end

  refine BigDecimal.singleton_class do
    def arbitrary
      lambda do
        "#{Fixnum.arbitrary.call}.#{Fixnum.arbitrary_natural.call}".to_d
      end
    end
  end

  refine BigDecimal do
    def shrink
      if self > 10 || -10 > self
        [self / 10]
      else
        []
      end
    end
  end
end
