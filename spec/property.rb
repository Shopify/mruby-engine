require "arbitrary"

module PropertyTesting
  using Arbitrary

  NUMBER_OF_CASES = 100

  def property(*generators)
    values = []
    NUMBER_OF_CASES.times do
      values = generators.map(&:call)
      begin
        yield(*values)
      rescue StandardError => first_error
        shrink_failure(values, first_error)
      end
    end
  end

  SHRINK_LIMIT = 1000

  def shrink_failure(values, first_error)
    shrunk_error = first_error
    SHRINK_LIMIT.times do
      values, shrunk_error = shrink_failure_once(values, shrunk_error)
    end
    raise shrunk_error
  end

  def shrink_failure_once(values, e)
    new_values = shrink_once(values)
    raise e if new_values.nil?

    begin
      yield(*new_values)
    rescue StandardError => candidate_e
      raise(e) unless e.class == candidate_e.class
      [new_values, candidate_e]
    else
      raise(e)
    end
  end

  def shrink_once(values)
    values.each_with_index do |value, index|
      new_value = value.shrink
      next if new_value.empty?
      new_values = values.dup
      new_values[index] = new_value[0]
      return new_values
    end
    nil
  end
end
