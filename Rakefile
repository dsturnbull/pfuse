require 'rubygems'
require 'rake'

begin
  require 'jeweler'
  Jeweler::Tasks.new do |gem|
    gem.name = "pfuse"
    gem.summary = %Q{fusefs}
    gem.description = %Q{Gemified}
    gem.email = "dsturnbull@me.com"
    gem.homepage = "http://github.com/dsturnbull/pfuse"
    gem.authors = ["David Turnbull", "Kyle Maxwell"]
    gem.extensions = ["ext/extconf.rb"]
  end
  Jeweler::GemcutterTasks.new
rescue LoadError
  puts "Jeweler (or a dependency) not available. Install it with: sudo gem install jeweler"
end

task :default => :build
