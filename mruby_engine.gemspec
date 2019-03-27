require "pathname"

lib = File.expand_path("../lib", __FILE__)
$LOAD_PATH.unshift(lib) unless $LOAD_PATH.include?(lib)

require "mruby_engine/version"

Gem::Specification.new do |spec|
  spec.name = "mruby_engine"
  spec.version = MRubyEngine::VERSION
  spec.authors = [
    "Jean Boussier",
    "John Duff",
    "Simon Génier",
    "Willem van der Jagt",
    "Alexandre Leblanc",
    "Guillaume Malette",
    "James Reid-Smith",
  ]
  spec.email = "webmaster@shopify.com"
  spec.summary = "A sandboxed scripting engine to customize Shopify's business logic."
  spec.homepage = "https://github.com/Shopify/mruby-engine"
  spec.license = "MIT"

  spec.files = begin
    submodules =
      %x(git submodule status --recursive).split("\n").map do |submodule|
        submodule.split(/\(|\s+/)[2]
      end.compact

    list_tracked_files = lambda do |dir|
      Dir.chdir(Pathname.new(__FILE__).dirname.join(dir)) do
        %x(git ls-files -z).split("\x0").map do |file|
          Pathname.new(dir).join(file).to_s
        end
      end
    end

    list_tracked_files.call(".") + submodules.flat_map do |submodule|
      list_tracked_files.call(submodule)
    end
  end
  spec.extensions = ["ext/mruby_engine/extconf.rb"]
  spec.test_files = spec.files.grep(%r{^spec/})
  spec.require_paths = ["lib"]
  spec.executables = ["mruby-engine-mirb"]

  spec.add_development_dependency "bundler", ">= 1.6"
  spec.add_development_dependency "rake", "~> 10.4"
  spec.add_development_dependency "rake-compiler", "~> 0.9"
  spec.add_development_dependency "rspec", "~> 3.3"
  spec.add_development_dependency "pry", "~> 0.10.0"
  spec.add_development_dependency "pry-byebug", "~> 3.1"
  spec.add_development_dependency "benchmark-ips", "~> 2.2"
end
