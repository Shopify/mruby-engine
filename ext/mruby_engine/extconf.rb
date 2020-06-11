require 'mkmf'
require 'pathname'
require 'rake'
require 'rubygems'
require_relative './flag_helper'

include FileUtils

Dir.chdir(Pathname.new(__dir__).join("../..")) do
  sh("script/mkmruby", "compile")
end

unless have_func("rb_thread_call_without_gvl")
  abort("rb_thread_call_without_gvl not found: do you have Ruby >= 2.0?")
end

unless find_header 'mruby.h', File.expand_path('../mruby/include/', __FILE__)
  abort 'missing mruby.h: did you clone the submodule?'
end

unless find_header 'libunwind.h'
  abort 'missing libunwind.h: did you install libunwind? Make sure to provision your vagrant.'
end

unless find_library 'mruby', 'mrb_open', File.expand_path('../mruby/build/sandbox/lib/', __FILE__)
  abort 'missing libmruby.a'
end

if RUBY_PLATFORM =~ /linux/
  unless find_library 'unwind', 'abort'
    abort 'missing libunwind.a: did you install libunwind? Make sure to provision your vagrant.'
  end
end

$CFLAGS << ' -std=gnu99 -fvisibility=hidden -Wno-declaration-after-statement '
$CFLAGS << " #{Flags.cflags.join(' ')} "
$CFLAGS << " #{Flags.defines.map { |define| "-D#{define}" }.join(' ')} "
$CFLAGS << " -Werror " if ENV["CI"]

create_makefile 'mruby_engine/mruby_engine'
