
ruby19 = ENV["RUBY19"] or raise "please set env RUBY19"
ruby20 = ENV["RUBY20"] or raise "please set env RUBY20"

require "fileutils"

FileUtils.mkpath "lib/1.9.1"
FileUtils.mkpath "lib/2.0.0"

system "cd lib\\1.9.1 && ( if not exist Makefile ( #{ruby19} ../../ext/extconf.rb ) ) && make" or exit $?.exitstatus
system "cd lib\\2.0.0 && ( if not exist Makefile ( #{ruby20} ../../ext/extconf.rb ) ) && make && strip -s *.so" or exit $?.exitstatus

Gem::Specification.new do |spec|
  spec.name = "extattr"
  spec.version = "0.1.2"
  spec.platform = "x86-mingw32"
  spec.summary = "extended attribute operation library for ruby"
  spec.license = "2-clause BSD License"
  spec.author = "dearblue"
  spec.email = "dearblue@users.sourceforge.jp"
  spec.homepage = "http://sourceforge.jp/projects/rutsubo/"
  spec.description = <<EOS
extattr is extended attribute operation library for ruby.
Supported for FreeBSD, Gnu/Linux and Microsoft Windows.
EOS
  spec.files = %w(
    README.txt
    LICENSE.txt
    ext/extconf.rb
    ext/extattr.c
    ext/extattr.bsd
    ext/extattr.linux
    ext/extattr.windows
    lib/extattr.rb
    lib/1.9.1/extattr.so
    lib/2.0.0/extattr.so
    rspecs/extattr.rb
  )

  spec.rdoc_options = %w(-e UTF-8 -m README.txt)
  spec.extra_rdoc_files = %w(README.txt LICENSE.txt ext/extattr.c)
  spec.has_rdoc = false
  spec.required_ruby_version = ">= 1.9.3"
end
