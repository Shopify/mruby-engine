require "spec_helper"

describe :raise_error do
  it "return stack of execution" do
    expect do
      LibunwindTest.raise_test_internal_error
    end.to raise_error(MRubyEngine::EngineInternalError) do |e|
      e.message.each_line.drop(1).each do |line|
        # 0x7f72a4ef5364 : (mspace_+0x7a4) [0x7f72a4ef5364]
        # (nil) : (+0x29) [(nil)]
        # 0x007f8deeb1ca8e : (invoke_block_from_c.part.51+0x24e) [0x007f8deeb1ca8e]
        expect(line).to match(/\(*\w+\)* : \(\w*(\.\w*)*\+0x\h*\) \[\(*\w+\)*\]/)
      end
    end
  end
end
