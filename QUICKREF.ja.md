# Quick Reference

メソッド名に『!』がついているものはシンボリックリンクに対する操作となります。

キーワード引数の `namespace` を与えると、拡張属性の名前空間を指定できます。

与えられる値:

  - `ExtAttr::USER`
  - `ExtAttr::SYSTEM`
  - `user` 又は `system` を文字列かシンボルで (大文字小文字は区別されない)

規定値は `ExtAttr::USER` です。


## モジュール `ExtAttr`

  - `ExtAttr.list(path, namespace) -> array`
  - `ExtAttr.list!(path, namespace) -> array`
  - `ExtAttr.size(path, namespace, name) -> integer`
  - `ExtAttr.size!(path, namespace, name) -> integer`
  - `ExtAttr.get(path, namespace, name) -> string`
  - `ExtAttr.get!(path, namespace, name) -> string`
  - `ExtAttr.set(path, namespace, name, value) -> nil`
  - `ExtAttr.set!(path, namespace, name, value) -> nil`
  - `ExtAttr.delete(path, namespace, name) -> nil`
  - `ExtAttr.delete!(path, namespace, name) -> nil`


## リファインメント `using ExtAttr`

リファインメント機能を使うことにより、`File` が拡張されます。

### 拡張属性の属性名を取得

```ruby:ruby
File.extattr_list(path, namespace: ExtAttr::USER) -> array
File.extattr_list(path, namespace: ExtAttr::USER) { |name| ... } -> nil
File.extattr_list!(path, namespace: ExtAttr::USER) -> array
File.extattr_list!(path, namespace: ExtAttr::USER) { |name| ... } -> nil
File#extattr_list(namespace: ExtAttr::USER) -> array
File#extattr_list(namespace: ExtAttr::USER) { |name| ... } -> nil
```

### 拡張属性の要素の大きさを取得

```ruby:ruby
File.extattr_size(path, name, namespace: ExtAttr::USER) -> size
File.extattr_size!(path, name, namespace: ExtAttr::USER) -> size
File#extattr_size(name, namespace: ExtAttr::USER) -> size
```

### 拡張属性の要素を取得

```ruby:ruby
File.extattr_get(path, name, namespace: ExtAttr::USER) -> data (String)
File.extattr_get!(path, name, namespace: ExtAttr::USER) -> data (String)
File#extattr_get(name, namespace: ExtAttr::USER) -> data (String)
```

### 拡張属性の要素を設定

```ruby:ruby
File.extattr_set(path, name, data, namespace: ExtAttr::USER) -> nil
File.extattr_set!(path, name, data, namespace: ExtAttr::USER) -> nil
File#extattr_set(name, data, namespace: ExtAttr::USER) -> nil
```

### 拡張属性の要素を削除

```ruby:ruby
File.extattr_delete(path, name, namespace: ExtAttr::USER) -> nil
File.extattr_delete!(path, name, namespace: ExtAttr::USER) -> nil
File#extattr_delete(name, namespace: ExtAttr::USER) -> nil
```

### 拡張属性の属性名と要素を列挙する

```ruby:ruby
File.extattr_each(path, namespace: ExtAttr::USER) -> Enumerator
File.extattr_each(path, namespace: ExtAttr::USER) { |name, data| ... } -> nil
File.extattr_each!(path, namespace: ExtAttr::USER) -> Enumerator
File.extattr_each!(path, namespace: ExtAttr::USER) { |name, data| ... } -> nil
File#extattr_each(namespace: ExtAttr::USER) -> Enumerator
File#extattr_each(namespace: ExtAttr::USER) { |name, data| ... } -> file
```
