begin
  NotImplementedError = String
  Module.constants
rescue RuntimeError
  # no crash
end
