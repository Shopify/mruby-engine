require "bundler/gem_tasks"
require "rake/extensiontask"

begin
  require "rspec/core/rake_task"
  RSpec::Core::RakeTask.new(:spec)
rescue LoadError
end

Rake::ExtensionTask.new("mruby_engine") do |ext|
  ext.lib_dir = "lib/mruby_engine"
end

Rake::Task[:clean].enhance do
  sh("script/mkmruby", "clean")
end

Rake::Task[:clobber].enhance do
  sh("script/mkmruby", "clobber")
end

task :"compile:mpdecimal:tests" do
  Dir.chdir("ext/mruby_engine/mruby-mpdecimal/tests/") do
    sh "make"
  end
end

task :"test:mpdecimal" => [:"compile:mpdecimal:tests"] do
  Dir.chdir("ext/mruby_engine/mruby-mpdecimal/tests/") do
    sh("./runtest", "official.decTest")
    sh("./runtest", "additional.decTest")
    sh("./runtest_alloc", "official.decTest")
  end
end

task :spec => [:compile]

task :default => [:spec, :"test:mpdecimal"]
