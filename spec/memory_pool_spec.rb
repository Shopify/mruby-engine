require "mruby_engine"

if ENV["MRUBY_ENGINE_NATIVE_TESTS"]
  describe :memory_pool do
    it "raises on user error" do
      expect do
        MemoryPoolTests.trigger_user_error!
      end.to raise_error(MRubyEngine::EngineInternalError, /user memory error/)
    end
  end
end
