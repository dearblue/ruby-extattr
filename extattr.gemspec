
Gem::Specification.new do |spec|
    spec.name = "extattr"
    spec.version = "0.1"
    spec.summary = "extended attribute operation library for ruby"
    spec.license = "2-clause BSD License"
    spec.author = "dearblue"
    spec.email = "dearblue@users.sourceforge.jp"
    spec.homepage = "http://sourceforge.jp/projects/rutsubo/"
    spec.description = <<-__END__
extattr is extended attribute operation library for ruby.
Supported for Microsoft Windows and FreeBSD.
    __END__
    spec.files = %w(
        README.txt
        LICENSE.txt
        ext/extconf.rb
        ext/extattr.c
        ext/extattr.bsd
        ext/extattr.linux
        ext/extattr.windows
        rspecs/extattr.rb
    )

    spec.rdoc_options = %w(-e UTF-8 -m README.txt)
    spec.extra_rdoc_files = %w(README.txt LICENSE.txt ext/extattr.c)
    spec.has_rdoc = false
    spec.required_ruby_version = ">= 1.9.3"
    spec.extensions << "ext/extconf.rb"
end
