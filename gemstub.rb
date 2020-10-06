unless version = File.read("README.md").scan(/^\s*[\*\-]\s*version:{1,2}\s*(.+)/i).flatten[-1]
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
  s.license = "BSD-2-Clause"
  s.author = "dearblue"
  s.email = "dearblue@users.osdn.me"

  s.add_development_dependency "rake", "~> 0"
end
