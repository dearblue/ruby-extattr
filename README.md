# extattr - filesystem extended attributes manipurator for ruby

extattr is filesystem extended attributes manipurator for ruby on FreeBSD, GNU/Linux and Microsoft Windows.

  * package name: [extattr](https://rubygems.org/gems/extattr)
  * Author: dearblue (mailto:dearblue@users.noreply.github.com)
  * Version: 0.3
  * Product quality: PROTOTYPE, UNSTABLE
  * License: [2-clause BSD License](LICENSE.md)
  * Project page: <https://github.com/dearblue/ruby-extattr>
  * Support ruby: ruby-2.3+

----

  * [HISTORY](HISTORY.ja.md) (In Japanese)

***END OF DOCUMENT IN ENGLISH***

----

***START OF DOCUMENT IN JAPANESE***

extattr はファイルシステムの拡張属性を操作するライブラリで、FreeBSD、GNU/Linux、Windows に対応しています。

サポートされる環境で、統一的なメソッドを提供します。

実装については、以下のようになっています。

  * FreeBSD: extattr (`extattr_list`, `extattr_get`, `extattr_set`, `extattr_delete`)
  * GNU/Linux: xattr (`listxattr`, `getxattr`, `setxattr`, `removexattr`)
  * Microsoft Windows: NTFS ADS (代替データストリーム) / 拡張ファイル属性 (`NtQueryEaFile`, `NtSetEaFile`)


## Tested system

  * PC-BSD/AMD64 10.0
  * lubuntu 13.10 (i386)
  * Microsoft Windows XP Professional SP3
  * Microsoft Windows 7 Professional SP1


## 簡易リファレンスマニュアル

クラスメソッドに『!』がついているものはシンボリックリンクに対する操作となります。

キーワード引数の ``namespace`` を与えると、拡張属性の名前空間を指定できます。

与えられる値:

  * ``ExtAttr::NAMESPACE_USER`` 又は ``File::EXTATTR_NAMESPACE_USER``
  * ``ExtAttr::NAMESPACE_SYSTEM`` 又は ``File::EXTATTR_NAMESPACE_SYSTEM``
  * ``user`` 又は ``system`` を文字列かシンボルで (大文字小文字は区別されない)

規定値は ``ExtAttr::NAMESPACE_USER`` です。


### 拡張属性の属性名を取得

``` ruby:ruby
File.extattr_list(path, namespace: File::EXTATTR_NAMESPACE_USER) -> array
File.extattr_list(path, namespace: File::EXTATTR_NAMESPACE_USER) { |name| ... } -> nil
File.extattr_list!(path, namespace: File::EXTATTR_NAMESPACE_USER) -> array
File.extattr_list!(path, namespace: File::EXTATTR_NAMESPACE_USER) { |name| ... } -> nil
File#extattr_list(namespace: File::EXTATTR_NAMESPACE_USER) -> array
File#extattr_list(namespace: File::EXTATTR_NAMESPACE_USER) { |name| ... } -> nil
```

### 拡張属性の要素の大きさを取得

``` ruby:ruby
File.extattr_size(path, name, namespace: File::EXTATTR_NAMESPACE_USER) -> size
File.extattr_size!(path, name, namespace: File::EXTATTR_NAMESPACE_USER) -> size
File#extattr_size(name, namespace: File::EXTATTR_NAMESPACE_USER) -> size
```

### 拡張属性の要素を取得

``` ruby:ruby
File.extattr_get(path, name, namespace: File::EXTATTR_NAMESPACE_USER) -> data (String)
File.extattr_get!(path, name, namespace: File::EXTATTR_NAMESPACE_USER) -> data (String)
File#extattr_get(name, namespace: File::EXTATTR_NAMESPACE_USER) -> data (String)
```

### 拡張属性の要素を設定

``` ruby:ruby
File.extattr_set(path, name, data, namespace: File::EXTATTR_NAMESPACE_USER) -> nil
File.extattr_set!(path, name, data, namespace: File::EXTATTR_NAMESPACE_USER) -> nil
File#extattr_set(name, data, namespace: File::EXTATTR_NAMESPACE_USER) -> nil
```

### 拡張属性の要素を削除

``` ruby:ruby
File.extattr_delete(path, name, namespace: File::EXTATTR_NAMESPACE_USER) -> nil
File.extattr_delete!(path, name, namespace: File::EXTATTR_NAMESPACE_USER) -> nil
File#extattr_delete(name, namespace: File::EXTATTR_NAMESPACE_USER) -> nil
```

### 拡張属性の属性名と要素を列挙する

``` ruby:ruby
File.extattr_each(path, namespace: File::EXTATTR_NAMESPACE_USER) -> Enumerator
File.extattr_each(path, namespace: File::EXTATTR_NAMESPACE_USER) { |name, data| ... } -> nil
File.extattr_each!(path, namespace: File::EXTATTR_NAMESPACE_USER) -> Enumerator
File.extattr_each!(path, namespace: File::EXTATTR_NAMESPACE_USER) { |name, data| ... } -> nil
File#extattr_each(namespace: File::EXTATTR_NAMESPACE_USER) -> Enumerator
File#extattr_each(namespace: File::EXTATTR_NAMESPACE_USER) { |name, data| ... } -> file
```


## GNU/Linux における特記事項

  * GNU/Linux で利用されている名前空間〈security〉〈trusted〉には未対応です。


## Microsoft Windows における特記事項

  * Windows XP 以降を必要とします。Win2k でも動作するかもしれません (未検証)。<br>
    Windows 9x シリーズでは `require "extattr"` の段階で例外が発生すると思われます (未検証)。
  * ``ExtAttr::NAMESPACE_USER`` は (比較的よく利用される) NTFS ADS に対する操作で、``ExtAttr::NAMESPACE_SYSTEM`` が本来の (あまり利用されていない) 拡張ファイル属性に対する操作となります。
  * リパースポイント (ジャンクションやシンボリックリンク) に対する NTFS ADS は要素の取得や設定、削除は出来ません。<br>
    必ずリンク先に対する操作となります (やり方がわかりません)。
  * 64 KiB を超える NTFS ADS は取得も設定も出来ません。<br>
    これは『拡張属性』と捉えた場合、巨大なデータを扱えるべきではないという考えによるためです
    (本当のところは FreeBSD の拡張属性が最大 64KiB 弱であることが由来です)。
  * 巨大な NTFS ADS を扱いたい場合は、``File.open`` でファイルとして扱えるので自由に読み書きできます。<br>
    これは ruby に限ったことではなく、Windows による仕様です。<br>
    この場合の与えるファイル名は、``path + ":" + name`` という形になります。

    ``` ruby:ruby
    filepath = "sample.txt"
    extattrname = "category"
    ntfs_ads_name = filepath + ":" + extattrname
    File.open(ntfs_ads_name, "r") do |file|
      ...
    end
    ```

## 既知のバグ

  * ``extattr_list`` のブロック内で ``extattr_set`` ``extattr_delete`` を行うと思いがけない副作用が起こる場合がある

    解決方法: ``extattr_set`` か ``extattr_delete`` のどちらかを同時に利用したい場合、``extattr_list.each`` を用いてください。


## 参考資料

  * extattr
      * <https://www.freebsd.org/cgi/man.cgi?query=extattr&sektion=2>
  * xattr
      * <https://linuxjm.osdn.jp/html/LDP_man-pages/man2/listxattr.2.html>
  * windows
      * <https://msdn.microsoft.com/en-us/library/cc232069.aspx>
      * <https://undocumented.ntinternals.net/UserMode/Undocumented%20Functions/NT%20Objects/File/FILE_INFORMATION_CLASS.html>
      * <https://undocumented.ntinternals.net/UserMode/Undocumented%20Functions/NT%20Objects/File/NtQueryInformationFile.html>
      * <https://undocumented.ntinternals.net/UserMode/Undocumented%20Functions/NT%20Objects/File/NtQueryEaFile.html>
      * <https://undocumented.ntinternals.net/UserMode/Undocumented%20Functions/NT%20Objects/File/NtSetEaFile.html>
      * FileTest <http://www.zezula.net/en/fstools/filetest.html> <https://github.com/ladislav-zezula/FileTest>
