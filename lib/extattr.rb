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
# * ExtAttr::USER, ExtAttr::SYSTEM
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
#   ExtAttr::USER の場合は NTFS Alternative Data Stream (ADS) として処理されます。
#
#   ExtAttr::SYSTEM の場合は NTFS Extended Attribute (EA) として処理されます。
#
module ExtAttr
  ExtAttr = self

  def self.open(path)
    if path.kind_of?(File)
      ea = ExtAttr::Accessor[path, path.to_path]
      block_given? ? yield(ea) : ea
    else
      if block_given?
        ::File.open(path) do |file|
          return yield(ExtAttr::Accessor[file, path])
        end
      else
        ExtAttr::Accessor[::File.open(path), path]
      end
    end
  end

  #
  # call-seq:
  #   each(path, namespace = ExtAttr::USER) -> Enumerator
  #   each(path, namespace = ExtAttr::USER) { |name| ... } -> path
  #
  def self.each(path, namespace = ExtAttr::USER, &block)
    return to_enum(:each, path, namespace) unless block

    list(path, namespace, &block)

    self
  end

  #
  # call-seq:
  #   each!(path, namespace = ExtAttr::USER) -> Enumerator
  #   each!(path, namespace = ExtAttr::USER) { |name| ... } -> path
  #
  def self.each!(path, namespace = ExtAttr::USER, &block)
    return to_enum(:each!, path, namespace) unless block

    list!(path, namespace, &block)

    self
  end

  #
  # call-seq:
  #   each_pair(path, namespace = ExtAttr::USER) -> Enumerator
  #   each_pair(path, namespace = ExtAttr::USER) { |name, data| ... } -> path
  #
  def self.each_pair(path, namespace = ExtAttr::USER, &block)
    return to_enum(:each_pair, path, namespace) unless block

    list(path, namespace) { |name| yield(name, get(path, namespace, name)) }

    self
  end

  #
  # call-seq:
  #   each_pair!(path, namespace = ExtAttr::USER) -> Enumerator
  #   each_pair!(path, namespace = ExtAttr::USER) { |name, data| ... } -> path
  #
  def self.each_pair!(path, namespace = ExtAttr::USER, &block)
    return to_enum(:each_pair!, path, namespace) unless block

    list!(path, namespace) { |name| yield(name, get!(path, namespace, name)) }

    self
  end

  class Accessor < Struct.new(:obj, :path)
    BasicStruct = superclass
    VIRT_ENOATTR = (ExtAttr::IMPLEMENT == "windows" ? Errno::ENOENT : Errno::ENOATTR)

    def [](name, namespace = ExtAttr::USER)
      begin
        ExtAttr.get(obj, namespace, name)
      rescue VIRT_ENOATTR
        nil
      end
    end

    def []=(name, namespace = ExtAttr::USER, data)
      if data.nil?
        ExtAttr.delete(obj, namespace, name)
      else
        ExtAttr.set(obj, namespace, name, data)
      end
    end

    def each(namespace: ExtAttr::USER, &block)
      ExtAttr.each(obj, namespace, &block)
    end

    def each_pair(namespace: ExtAttr::USER, &block)
      ExtAttr.each_pair(obj, namespace, &block)
    end

    def list(namespace: ExtAttr::USER, &block)
      ExtAttr.list(obj, namespace, &block)
    end

    def size(name, namespace: ExtAttr::USER)
      ExtAttr.size(obj, namespace, name)
    end

    def get(name, namespace: ExtAttr::USER)
      ExtAttr.get(obj, namespace, name)
    end

    def set(name, data, namespace: ExtAttr::USER)
      ExtAttr.set(obj, namespace, name, data)
    end

    def delete(name, namespace: ExtAttr::USER)
      ExtAttr.delete(obj, namespace, name)
    end
  end

  refine File do
    def extattr
      ExtAttr::Accessor[self, to_path]
    end

    #
    # Enumeration file extattr.
    #
    def extattr_each(namespace: ExtAttr::USER, &block)
      ExtAttr.each(self, namespace, &block)
    end

    #
    # Enumeration file extattr.
    #
    def extattr_each_pair(namespace: ExtAttr::USER, &block)
      ExtAttr.each_pair(self, namespace, &block)
    end

    #
    # call-seq:
    #   extattr_list(namespace: ExtAttr::USER) -> array of strings
    #   extattr_list(namespace: ExtAttr::USER) { |name| ... } -> enumerator
    #
    # Get file extattr list.
    #
    def extattr_list(namespace: ExtAttr::USER, &block)
      ExtAttr.list(self, namespace, &block)
    end

    #
    # Get file extattr data size.
    #
    def extattr_size(name, namespace: ExtAttr::USER)
      ExtAttr.size(self, namespace, name)
    end

    #
    # Get file extattr data.
    #
    def extattr_get(name, namespace: ExtAttr::USER)
      ExtAttr.get(self, namespace, name)
    end

    #
    # Set file extattr data.
    #
    def extattr_set(name, value, namespace: ExtAttr::USER)
      ExtAttr.set(self, namespace, name, value)
    end

    #
    # Delete file extattr.
    #
    def extattr_delete(name, namespace: ExtAttr::USER)
      ExtAttr.delete(self, namespace, name)
    end
  end

  refine File.singleton_class do
    def extattr(path)
      ExtAttr.open(path)
    end

    def extattr_each(path, namespace: ExtAttr::USER, &block)
      ExtAttr.each(path, namespace, &block)
    end

    def extattr_each!(path, namespace: ExtAttr::USER, &block)
      ExtAttr.each!(path, namespace, &block)
    end

    def extattr_each_pair(path, namespace: ExtAttr::USER, &block)
      ExtAttr.each_pair(path, namespace, &block)
    end

    def extattr_each_pair!(path, namespace: ExtAttr::USER, &block)
      ExtAttr.each_pair!(path, namespace, &block)
    end

    def extattr_list(path, namespace: ExtAttr::USER, &block)
      ExtAttr.list(path, namespace, &block)
    end

    def extattr_list!(path, namespace: ExtAttr::USER, &block)
      ExtAttr.list!(path, namespace, &block)
    end

    def extattr_get(path, name, namespace: ExtAttr::USER)
      ExtAttr.get(path, namespace, name)
    end

    def extattr_get!(path, name, namespace: ExtAttr::USER)
      ExtAttr.get(path, namespace, name)
    end

    def extattr_size(path, name, namespace: ExtAttr::USER)
      ExtAttr.size(path, namespace, name)
    end

    def extattr_size!(path, name, namespace: ExtAttr::USER)
      ExtAttr.size(path, namespace, name)
    end

    def extattr_set(path, name, value, namespace: ExtAttr::USER)
      ExtAttr.set(path, namespace, name, value)
    end

    def extattr_set!(path, name, value, namespace: ExtAttr::USER)
      ExtAttr.set(path, namespace, name, value)
    end

    def extattr_delete(path, name, namespace: ExtAttr::USER)
      ExtAttr.delete(path, namespace, name)
    end

    def extattr_delete!(path, name, namespace: ExtAttr::USER)
      ExtAttr.delete(path, namespace, name)
    end
  end
end
