#vim: set fileencoding:utf-8

ver = RbConfig::CONFIG["ruby_version"]
soname = File.basename(__FILE__, ".rb") << ".so"
lib = File.join(File.dirname(__FILE__), ver, soname)
if File.file?(lib)
  require_relative File.join(ver, soname)
else
  require_relative soname
end
