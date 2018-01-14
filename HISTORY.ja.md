
# extattr-0.3

  * ExtAttr モジュールを追加して実装をそちらに移動
      * File クラスは ExtAttr::Extentions::File を include し、
        〜〜::FileClass を extend するように変更しました。
  * ExtAttr::Accessor クラスの追加
      * 拡張属性の操作に特化したオブジェクトを扱えるようになりました。
  * bin/extattr コマンドラインプログラムの追加
  * mingw でライブラリを構築する時 C 実行時ライブラリを静的リンクするように変更

# extattr-0.2

  * セーフレベルに関する改善
      * `$SAFE` と汚染状態を確認するようになりました
      * 汚染状態を伝播させるようになりました
  * バージョン情報と実装 API に関する情報の取得
      * `File::EXTATTR_VERSION` 定数によって、バージョン情報が取得できるようになりました
      * `File::EXTATTR_IMPLEMENT` 定数によって、実装に関する情報が取得できるようになりました
  * 機能の改善
      * `File.extattr_each` / `File#extattr_each` によって、ファイル拡張属性の名前と値を繰り返し取得できるようになりました
  * Windows 版に関する改善
      * `File::EXTATTR_NAMESPACE_SYSTEM` によって、本来の拡張ファイル属性の操作が行えるようになりました
      (NtQueryEaFile / NtSetEaFile)

# extattr-0.1.2

# extattr-0.1.1

# extattr-0.1

はじめてのりりーす

  * 拡張ファイル属性の一覧、取得、設定、削除を行うことが出来ます
  * **セキュリティリスクあり**: `$SAFE` の確認や汚染状態の伝播を行ないません
  * Windows 版では、NTFS ADS に対する操作となります。`File::EXTATTR_NAMESPACE_USER` のみが有効です
