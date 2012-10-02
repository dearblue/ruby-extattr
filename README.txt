= extattr

extattr is extended attribute operation library for Windows and FreeBSD.

----

extattr は拡張属性を操作するライブラリで、Windows、FreeBSD に対応しています (GNU/Linux はコードを書いてみただけで、試験がまったくされていません)。

サポートされる環境で、統一的なメソッドを提供します。

クラスメソッドに『!』がついているものはシンボリックリンクに対する操作となります。

キーワード引数の <code>namespace</code> を与えると、拡張属性の名前空間を指定できます。規定値は <code>EXTATTR_NAMESPACE_USER</code> で、ほかの値は <code>EXTATTR_NAMESPACE_SYSTEM</code> のみが指定できます (Windows 版では <code>EXTATTR_NAMESPACE_USER</code> のみが指定可能です)。


== 拡張属性の属性名を取得:

- File#extattr_list(namespace: File::EXTATTR_NAMESPACE_USER) -> array
- File.extattr_list(path, namespace: File::EXTATTR_NAMESPACE_USER) -> array
- File.extattr_list!(path, namespace: File::EXTATTR_NAMESPACE_USER) -> array

== 拡張属性の要素の大きさを取得:

- File#extattr_size(name, namespace: File::EXTATTR_NAMESPACE_USER) -> size
- File.extattr_size(path, name, namespace: File::EXTATTR_NAMESPACE_USER) -> size
- File.extattr_size!(path, name, namespace: File::EXTATTR_NAMESPACE_USER) -> size

== 拡張属性の要素を取得:

- File#extattr_get(name, namespace: File::EXTATTR_NAMESPACE_USER) -> data (String)
- File.extattr_get(path, name, namespace: File::EXTATTR_NAMESPACE_USER) -> data (String)
- File.extattr_get!(path, name, namespace: File::EXTATTR_NAMESPACE_USER) -> data (String)

== 拡張属性の要素を設定:

- File#extattr_set(name, data, namespace: File::EXTATTR_NAMESPACE_USER) -> nil
- File.extattr_set(path, name, data, namespace: File::EXTATTR_NAMESPACE_USER) -> nil
- File.extattr_set!(path, name, data, namespace: File::EXTATTR_NAMESPACE_USER) -> nil

== 拡張属性の要素を削除:

- File#extattr_delete(name, namespace: File::EXTATTR_NAMESPACE_USER) -> nil
- File.extattr_delete(path, name, namespace: File::EXTATTR_NAMESPACE_USER) -> nil
- File.extattr_delete!(path, name, namespace: File::EXTATTR_NAMESPACE_USER) -> nil


== Microsoft Windows における諸注意

Windows 2000 以降でのみ動作します。Windows 9X シリーズでは <code>require "extattr"</code> の段階で例外が発生するでしょう。

リパースポイント (ジャンクションやシンボリックリンク) に対する ADS は要素の取得や設定、削除は出来ません。必ずリンク先に対する操作となります。

128 KiB を超える ADS は取得も設定も出来ません。これは『拡張属性』と捉えた場合、巨大なデータを扱えるべきではないという考えによるためです (本当のところは FreeBSD の拡張属性が最大 64KiB 弱であることが由来です)。巨大な ADS を扱いたい場合は、File.open で自由に読み書きできます (これは ruby に限ったことではなく、Windows による仕様です)。この場合の与えるファイル名は、path + ":" + name という形になります。
