if File.read("README.md", 4096) =~ /^\s*\*\s*version:{1,2}\s*(.+)/i
  version = $1
else
  raise "バージョン情報が README.md に見つかりません"
end


GEMSTUB = Gem::Specification.new do |s|
  s.name = "extattr"
  s.version = version
  s.summary = "extended file attribute manipurator"
  s.description = <<-EOS
"extattr" is extended file attribute manipurator for ruby.
Supported for FreeBSD (extattr), GNU/Linux (xattr) and Microsoft Windows (NTFS Alternative Data Stream (ADS) + NTFS Extended Attributes (EA)).
  EOS
  s.homepage = "https://github.com/dearblue/ruby-extattr"
  s.license = "BSD 2-clause License"
  s.author = "dearblue"
  s.email = "dearblue@users.noreply.github.com"

  s.add_development_dependency "rake", "~> 0"
end
