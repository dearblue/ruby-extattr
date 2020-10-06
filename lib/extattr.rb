#!ruby

ver = RUBY_VERSION.slice(/\d+\.\d+/)
soname = File.basename(__FILE__, ".rb") << ".so"
lib = File.join(ver, soname)
begin
  require_relative lib
rescue LoadError
  require lib
end

#
# Add operation methods of filesystem extended attributes to File.
#
# === 名前空間の指定について
#
# 拡張属性の名前空間を指定する場合、以下の値が利用できます:
#
# * ExtAttr::NAMESPACE_USER, ExtAttr::NAMESPACE_SYSTEM、
#   File::EXTATTR_NAMESPACE_USER、File::EXTATTR_NAMESPACE_SYSTEM
# * 文字列又はシンボルで +user+、+system+ (大文字小文字を区別しません)
#
# これらの値は内部で変換、または処理が分岐されます。
#
# extattr::
#   整数値に変換されて処理されます。
#
# xattr::
#   拡張属性名に "user." または "system." を追加して処理されます。
#
# Windows::
#   NAMESPACE_USER の場合は NTFS Alternative Data Stream (ADS) として処理されます。
#
#   NAMESPACE_SYSTEM の場合は NTFS Extended Attribute (EA) として処理されます。
#
module ExtAttr
  ExtAttr = self

  def self.open(path, namespace: NAMESPACE_USER)
    if path.kind_of?(File)
      ExtAttr::Accessor[path, path.to_path, namespace]
    else
      ExtAttr::Accessor[::File.open(path), path, namespace]
    end
  end

  #
  # call-seq:
  #   each(path, namespace) -> Enumerator
  #   each(path, namespace) { |name, data| ... } -> path
  #
  def self.each(path, namespace, &block)
    return to_enum(:each, path, namespace) unless block

    list(path, namespace) { |name| yield(name, get(path, namespace, name)) }

    self
  end

  #
  # call-seq:
  #   each!(path, namespace) -> Enumerator
  #   each!(path, namespace) { |name, data| ... } -> path
  #
  def self.each!(path, namespace, &block)
    return to_enum(:each!, path, namespace) unless block

    list!(path, namespace) { |name| yield(name, get!(path, namespace, name)) }

    self
  end

  class Accessor < Struct.new(:obj, :path, :namespace)
    def [](name)
      ExtAttr.get(obj, namespace, name)
    end

    def []=(name, data)
      if data.nil?
        ExtAttr.delete(obj, namespace, name)
      else
        ExtAttr.set(obj, namespace, name, data)
      end
    end

    def each(&block)
      ExtAttr.each(obj, namespace, &block)
    end

    def list(&block)
      ExtAttr.list(obj, namespace, &block)
    end

    def size(name)
      ExtAttr.size(obj, namespace, name)
    end

    def get(name)
      ExtAttr.get(obj, namespace, name)
    end

    def set(name, data)
      ExtAttr.set(obj, namespace, name, data)
    end

    def delete(name)
      ExtAttr.delete(obj, namespace, name)
    end
  end

  module File
    module Constants
      EXTATTR_NAMESPACE_USER = ExtAttr::NAMESPACE_USER
      EXTATTR_NAMESPACE_SYSTEM = ExtAttr::NAMESPACE_SYSTEM
      EXTATTR_IMPLEMENT = ExtAttr::IMPLEMENT
    end

    include Constants

    def extattr(namespace: EXTATTR_NAMESPACE_USER)
      ExtAttr::Accessor[self, to_path, namespace]
    end

    #
    # Enumeration file extattr.
    #
    def extattr_each(namespace: ExtAttr::NAMESPACE_USER, &block)
      ExtAttr.each(self, namespace, &block)
    end

    #
    # call-seq:
    #   extattr_list(namespace: File::EXTATTR_NAMESPACE_USER) -> array of strings
    #   extattr_list(namespace: File::EXTATTR_NAMESPACE_USER) { |name| ... } -> enumerator
    #
    # Get file extattr list.
    #
    def extattr_list(namespace: ExtAttr::NAMESPACE_USER, &block)
      ExtAttr.list(self, namespace, &block)
    end

    #
    # Get file extattr data size.
    #
    def extattr_size(name, namespace: ExtAttr::NAMESPACE_USER)
      ExtAttr.size(self, namespace, name)
    end

    #
    # Get file extattr data.
    #
    def extattr_get(name, namespace: ExtAttr::NAMESPACE_USER)
      ExtAttr.get(self, namespace, name)
    end

    #
    # Set file extattr data.
    #
    def extattr_set(name, value, namespace: ExtAttr::NAMESPACE_USER)
      ExtAttr.set(self, namespace, name, value)
    end

    #
    # Delete file extattr.
    #
    def extattr_delete(name, namespace: ExtAttr::NAMESPACE_USER)
      ExtAttr.delete(self, namespace, name)
    end
  end

  module FileClass
    include ExtAttr::File::Constants

    def extattr(path, namespace: EXTATTR_NAMESPACE_USER)
      ExtAttr.open(path, namespace: namespace)
    end

    def extattr_each(path, namespace: ExtAttr::NAMESPACE_USER, &block)
      ExtAttr.each(path, namespace, &block)
    end

    def extattr_each!(path, namespace: ExtAttr::NAMESPACE_USER, &block)
      ExtAttr.each!(path, namespace, &block)
    end

    def extattr_list(path, namespace: ExtAttr::NAMESPACE_USER, &block)
      ExtAttr.list(path, namespace, &block)
    end

    def extattr_list!(path, namespace: ExtAttr::NAMESPACE_USER, &block)
      ExtAttr.list!(path, namespace, &block)
    end

    def extattr_get(path, name, namespace: ExtAttr::NAMESPACE_USER)
      ExtAttr.get(path, namespace, name)
    end

    def extattr_get!(path, name, namespace: ExtAttr::NAMESPACE_USER)
      ExtAttr.get(path, namespace, name)
    end

    def extattr_size(path, name, namespace: ExtAttr::NAMESPACE_USER)
      ExtAttr.size(path, namespace, name)
    end

    def extattr_size!(path, name, namespace: ExtAttr::NAMESPACE_USER)
      ExtAttr.size(path, namespace, name)
    end

    def extattr_set(path, name, value, namespace: ExtAttr::NAMESPACE_USER)
      ExtAttr.set(path, namespace, name, value)
    end

    def extattr_set!(path, name, value, namespace: ExtAttr::NAMESPACE_USER)
      ExtAttr.set(path, namespace, name, value)
    end

    def extattr_delete(path, name, namespace: ExtAttr::NAMESPACE_USER)
      ExtAttr.delete(path, namespace, name)
    end

    def extattr_delete!(path, name, namespace: ExtAttr::NAMESPACE_USER)
      ExtAttr.delete(path, namespace, name)
    end
  end
end

class File
  include ExtAttr::File
  extend ExtAttr::FileClass
end
