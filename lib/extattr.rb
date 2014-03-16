#vim: set fileencoding:utf-8

ver = RbConfig::CONFIG["ruby_version"]
soname = File.basename(__FILE__, ".rb") << ".so"
lib = File.join(File.dirname(__FILE__), ver, soname)
if File.file?(lib)
  require_relative File.join(ver, soname)
else
  require_relative soname
end

#
# Add operation methods of extended file attribute to File.
#
# File クラスに拡張属性を操作するメソッドを追加します。
#
# 感嘆符 (『!』) のついたメソッドは、シンボリックリンクに対する操作となります。
#
# メソッドにキーワード引数として <code>namespace:</code> を与えることにより、拡張属性の名前空間を指定することが出来ます。
#
# 現在の実装においては <code>EXTATTR_NAMESPACE_USER</code> と <code>EXTATTR_NAMESPACE_SYSTEM</code> のみが利用可能です。
#
class File
  #
  # call-seq:
  #   extattr_each(path, namespace: File::EXTATTR_NAMESPACE_USER) -> Enumerator
  #   extattr_each(path, namespace: File::EXTATTR_NAMESPACE_USER) { |name, data| ... } -> File
  #   extattr_each!(path, namespace: File::EXTATTR_NAMESPACE_USER) -> Enumerator
  #   extattr_each!(path, namespace: File::EXTATTR_NAMESPACE_USER) { |name, data| ... } -> File
  #
  def self.extattr_each(path, *namespace)
    return to_enum(:extattr_each, path, *namespace) unless block_given?

    extattr_list(path, *namespace) do |name|
      yield(name, extattr_get(path, name, *namespace))
    end
    self
  end

  def self.extattr_each!(path, *namespace)
    return to_enum(:extattr_each!, path, *namespace) unless block_given?

    extattr_list!(path, *namespace) do |name|
      yield(name, extattr_get!(path, name, *namespace))
    end
    self
  end

  def extattr_each(*namespace)
    return to_enum(:extattr_each, *namespace) unless block_given?

    extattr_list(*namespace) do |name|
      yield(name, extattr_get(name, *namespace))
    end
    self
  end
end
