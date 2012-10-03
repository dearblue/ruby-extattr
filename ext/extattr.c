/* encoding:utf-8 */

/*
 * extattr.c
 * AUTHOR:: dearblue <dearblue@zoho.com>
 * LICENSE:: 2-clause BSD License
 * WWW:: http://sourceforge.jp/projects/rutsubo
 */

#include <ruby.h>
#include <ruby/io.h>
#include <ruby/intern.h>


static VALUE file_extattr_list0(VALUE file, int fd, int namespace);
static VALUE file_extattr_size0(VALUE file, int fd, int namespace, VALUE name);
static VALUE file_extattr_get0(VALUE file, int fd, int namespace, VALUE name);
static VALUE file_extattr_set0(VALUE file, int fd, int namespace, VALUE name, VALUE data);
static VALUE file_extattr_delete0(VALUE file, int fd, int namespace, VALUE name);
static VALUE file_s_extattr_list0(VALUE path, int namespace);
static VALUE file_s_extattr_list_link0(VALUE path, int namespace);
static VALUE file_s_extattr_size0(VALUE path, int namespace, VALUE name);
static VALUE file_s_extattr_size_link0(VALUE path, int namespace, VALUE name);
static VALUE file_s_extattr_get0(VALUE path, int namespace, VALUE name);
static VALUE file_s_extattr_get_link0(VALUE path, int namespace, VALUE name);
static VALUE file_s_extattr_set0(VALUE path, int namespace, VALUE name, VALUE data);
static VALUE file_s_extattr_set_link0(VALUE path, int namespace, VALUE name, VALUE data);
static VALUE file_s_extattr_delete0(VALUE path, int namespace, VALUE name);
static VALUE file_s_extattr_delete_link0(VALUE path, int namespace, VALUE name);

// Init_extattr から呼び出される、環境ごとの初期設定。
static void setup(void);


#define RDOC(...)

#define ELEMENTOF(V) (sizeof(V) / sizeof((V)[0]))


static inline VALUE
hash_lookup(VALUE hash, VALUE key, VALUE default_value)
{
    if (NIL_P(hash)) {
        return default_value;
    } else if (rb_obj_is_kind_of(hash, rb_cHash)) {
        return rb_hash_lookup2(hash, key, default_value);
    } else {
        rb_raise(rb_eTypeError, "not hash");
    }
}

static inline int
file2fd(VALUE file)
{
    rb_io_t *fptr;
    // FIXME: ファイルであることを確認すべき
    GetOpenFile(file, fptr);
    return fptr->fd;
}


#if defined(HAVE_SYS_EXTATTR_H)
#   include "extattr.bsd"
#elif defined(HAVE_WINNT_H)
#   include "extattr.windows"
#elif defined(HAVE_ATTR_XATTR_H) || defined(HAVE_SYS_XATTR_H)
#   include "extattr.linux"
#else
#   error ruby extattr not supported on your system
#endif


static VALUE SYMnamespace;


/*
 * call-seq:
 * file.extattr_list(namespace: File::EXTATTR_NAMESPACE_USER) -> names array
 * file.extattr_list(namespace: File::EXTATTR_NAMESPACE_USER) { |name| ... } -> nil
 *
 * 開いているファイルの拡張属性名一覧を得ます。
 *
 * ブロックが指定された場合、一つ一つの拡張属性名を渡してブロックが評価されます。ブロックの戻り値は無視されます。
 */
static VALUE
file_extattr_list(int argc, VALUE argv[], VALUE file)
{
    VALUE opts = Qnil;
    rb_scan_args(argc, argv, "0:", &opts);
    return file_extattr_list0(file, file2fd(file),
                              NUM2INT(hash_lookup(opts, SYMnamespace, INT2NUM(EXTATTR_NAMESPACE_USER))));
}

/*
 * call-seq:
 * File.extattr_list(path, namespace: File::EXTATTR_NAMESPACE_USER) -> names array
 * File.extattr_list(path, namespace: File::EXTATTR_NAMESPACE_USER) { |name| ... } -> nil
 *
 * ファイル名を指定すること以外は File#extattr_list と同じです。
 */
static VALUE
file_s_extattr_list(int argc, VALUE argv[], VALUE file)
{
    VALUE path, opts = Qnil;
    rb_scan_args(argc, argv, "1:", &path, &opts);
    return file_s_extattr_list0(StringValue(path),
                                NUM2INT(hash_lookup(opts, SYMnamespace, INT2NUM(EXTATTR_NAMESPACE_USER))));
}

/*
 * call-seq:
 * File.extattr_list!(path, namespace: File::EXTATTR_NAMESPACE_USER) -> names array
 * File.extattr_list!(path, namespace: File::EXTATTR_NAMESPACE_USER) { |name| ... } -> nil
 *
 * シンボリックリンクに対する操作という以外は、File.extattr_list と同じです。
 */
static VALUE
file_s_extattr_list_link(int argc, VALUE argv[], VALUE file)
{
    VALUE path, opts = Qnil;
    rb_scan_args(argc, argv, "1:", &path, &opts);
    return file_s_extattr_list_link0(StringValue(path),
                                     NUM2INT(hash_lookup(opts, SYMnamespace, INT2NUM(EXTATTR_NAMESPACE_USER))));
}


/*
 * call-seq:
 * file.extattr_size(name, namespace: File::EXTATTR_NAMESPACE_USER) -> names array
 *
 * 拡張属性の大きさを取得します。
 */
static VALUE
file_extattr_size(int argc, VALUE argv[], VALUE file)
{
    VALUE name, opts = Qnil;
    rb_scan_args(argc, argv, "1:", &name, &opts);
    return file_extattr_size0(file, file2fd(file),
                              NUM2INT(hash_lookup(opts, SYMnamespace, INT2NUM(EXTATTR_NAMESPACE_USER))),
                              StringValue(name));
}

/*
 * call-seq:
 * File.extattr_size(path, name, namespace: File::EXTATTR_NAMESPACE_USER) -> size
 */
static VALUE
file_s_extattr_size(int argc, VALUE argv[], VALUE file)
{
    VALUE path, name, opts = Qnil;
    rb_scan_args(argc, argv, "2:", &path, &name, &opts);
    return file_s_extattr_size0(StringValue(path),
                                NUM2INT(hash_lookup(opts, SYMnamespace, INT2NUM(EXTATTR_NAMESPACE_USER))),
                                StringValue(name));
}

/*
 * call-seq:
 * File.extattr_size!(path, name, namespace: File::EXTATTR_NAMESPACE_USER) -> size
 */
static VALUE
file_s_extattr_size_link(int argc, VALUE argv[], VALUE file)
{
    VALUE path, name, opts = Qnil;
    rb_scan_args(argc, argv, "2:", &path, &name, &opts);
    return file_s_extattr_size_link0(StringValue(path),
                                     NUM2INT(hash_lookup(opts, SYMnamespace, INT2NUM(EXTATTR_NAMESPACE_USER))),
                                     StringValue(name));
}


/*
 * call-seq:
 * file.extattr_get(name, namespace: File::EXTATTR_NAMESPACE_USER) -> data
 */
static VALUE
file_extattr_get(int argc, VALUE argv[], VALUE file)
{
    VALUE name, opts = Qnil;
    rb_scan_args(argc, argv, "1:", &name, &opts);
    return file_extattr_get0(file, file2fd(file),
                             NUM2INT(hash_lookup(opts, SYMnamespace, INT2NUM(EXTATTR_NAMESPACE_USER))),
                             StringValue(name));
}

/*
 * call-seq:
 * File.extattr_get(path, name, namespace: File::EXTATTR_NAMESPACE_USER) -> data
 */
static VALUE
file_s_extattr_get(int argc, VALUE argv[], VALUE file)
{
    VALUE path, name, opts = Qnil;
    rb_scan_args(argc, argv, "2:", &path, &name, &opts);
    return file_s_extattr_get0(StringValue(path),
                               NUM2INT(hash_lookup(opts, SYMnamespace, INT2NUM(EXTATTR_NAMESPACE_USER))),
                               StringValue(name));
}

/*
 * call-seq:
 * File.extattr_get!(path, name, namespace: File::EXTATTR_NAMESPACE_USER) -> data
 */
static VALUE
file_s_extattr_get_link(int argc, VALUE argv[], VALUE file)
{
    VALUE path, name, opts = Qnil;
    rb_scan_args(argc, argv, "2:", &path, &name, &opts);
    return file_s_extattr_get_link0(StringValue(path),
                                    NUM2INT(hash_lookup(opts, SYMnamespace, INT2NUM(EXTATTR_NAMESPACE_USER))),
                                    StringValue(name));
}


/*
 * call-seq:
 * file.extattr_set(name, data, namespace: File::EXTATTR_NAMESPACE_USER) -> nil
 */
static VALUE
file_extattr_set(int argc, VALUE argv[], VALUE file)
{
    VALUE name, data, opts = Qnil;
    rb_scan_args(argc, argv, "11:", &name, &data, &opts);
    return file_extattr_set0(file, file2fd(file),
                             NUM2INT(hash_lookup(opts, SYMnamespace, INT2NUM(EXTATTR_NAMESPACE_USER))),
                             StringValue(name),
                             StringValue(data));
}

/*
 * call-seq:
 * File.extattr_set(path, name, data, namespace: File::EXTATTR_NAMESPACE_USER) -> nil
 */
static VALUE
file_s_extattr_set(int argc, VALUE argv[], VALUE file)
{
    VALUE path, name, data, opts = Qnil;
    rb_scan_args(argc, argv, "21:", &path, &name, &data, &opts);
    return file_s_extattr_set0(StringValue(path),
                               NUM2INT(hash_lookup(opts, SYMnamespace, INT2NUM(EXTATTR_NAMESPACE_USER))),
                               StringValue(name),
                               StringValue(data));
}

/*
 * call-seq:
 * File.extattr_set!(path, name, data, namespace: File::EXTATTR_NAMESPACE_USER) -> nil
 */
static VALUE
file_s_extattr_set_link(int argc, VALUE argv[], VALUE file)
{
    VALUE path, name, data, opts = Qnil;
    rb_scan_args(argc, argv, "21:", &path, &name, &data, &opts);
    return file_s_extattr_set_link0(StringValue(path),
                                    NUM2INT(hash_lookup(opts, SYMnamespace, INT2NUM(EXTATTR_NAMESPACE_USER))),
                                    StringValue(name),
                                    StringValue(data));
}


/*
 * call-seq:
 * file.extattr_delete(name, namespace: File::EXTATTR_NAMESPACE_USER) -> nil
 */
static VALUE
file_extattr_delete(int argc, VALUE argv[], VALUE file)
{
    VALUE name, opts = Qnil;
    rb_scan_args(argc, argv, "1:", &name, &opts);
    return file_extattr_delete0(file, file2fd(file),
                                NUM2INT(hash_lookup(opts, SYMnamespace, INT2NUM(EXTATTR_NAMESPACE_USER))),
                                StringValue(name));
}

/*
 * call-seq:
 * File.extattr_delete(path, name, namespace: File::EXTATTR_NAMESPACE_USER) -> nil
 */
static VALUE
file_s_extattr_delete(int argc, VALUE argv[], VALUE file)
{
    VALUE path, name, opts = Qnil;
    rb_scan_args(argc, argv, "2:", &path, &name, &opts);
    return file_s_extattr_delete0(StringValue(path),
                                  NUM2INT(hash_lookup(opts, SYMnamespace, INT2NUM(EXTATTR_NAMESPACE_USER))),
                                  StringValue(name));
}

/*
 * call-seq:
 * File.extattr_delete!(path, name, namespace: File::EXTATTR_NAMESPACE_USER) -> nil
 */
static VALUE
file_s_extattr_delete_link(int argc, VALUE argv[], VALUE file)
{
    VALUE path, name, opts = Qnil;
    rb_scan_args(argc, argv, "2:", &path, &name, &opts);
    return file_s_extattr_delete_link0(StringValue(path),
                                       NUM2INT(hash_lookup(opts, SYMnamespace, INT2NUM(EXTATTR_NAMESPACE_USER))),
                                       StringValue(name));
}


/*
 * Document-class: File
 *
 * File クラスに拡張属性を操作するメソッドを追加します。
 *
 * 感嘆符(『!』)のついたメソッドは、シンボリックリンクに対する操作となります。
 *
 * メソッドにキーワード引数として <code>namespace:</code> を与えることにより、拡張属性の名前空間を指定することが出来ます。
 * 現在の実装においては <code>EXTATTR_NAMESPACE_USER</code> と <code>EXTATTR_NAMESPACE_SYSTEM</code> のみが利用可能です。
 */


void
Init_extattr(void)
{
    SYMnamespace = ID2SYM(rb_intern("namespace"));

    rb_const_set(rb_cFile, rb_intern("EXTATTR_NAMESPACE_USER"), INT2NUM(EXTATTR_NAMESPACE_USER));
    rb_const_set(rb_cFile, rb_intern("EXTATTR_NAMESPACE_SYSTEM"), INT2NUM(EXTATTR_NAMESPACE_SYSTEM));

    rb_define_method(rb_cFile, "extattr_list", RUBY_METHOD_FUNC(file_extattr_list), -1);
    rb_define_singleton_method(rb_cFile, "extattr_list", RUBY_METHOD_FUNC(file_s_extattr_list), -1);
    rb_define_singleton_method(rb_cFile, "extattr_list!", RUBY_METHOD_FUNC(file_s_extattr_list_link), -1);

    rb_define_method(rb_cFile, "extattr_size", RUBY_METHOD_FUNC(file_extattr_size), -1);
    rb_define_singleton_method(rb_cFile, "extattr_size", RUBY_METHOD_FUNC(file_s_extattr_size), -1);
    rb_define_singleton_method(rb_cFile, "extattr_size!", RUBY_METHOD_FUNC(file_s_extattr_size_link), -1);

    rb_define_method(rb_cFile, "extattr_get", RUBY_METHOD_FUNC(file_extattr_get), -1);
    rb_define_singleton_method(rb_cFile, "extattr_get", RUBY_METHOD_FUNC(file_s_extattr_get), -1);
    rb_define_singleton_method(rb_cFile, "extattr_get!", RUBY_METHOD_FUNC(file_s_extattr_get_link), -1);

    rb_define_method(rb_cFile, "extattr_set", RUBY_METHOD_FUNC(file_extattr_set), -1);
    rb_define_singleton_method(rb_cFile, "extattr_set", RUBY_METHOD_FUNC(file_s_extattr_set), -1);
    rb_define_singleton_method(rb_cFile, "extattr_set!", RUBY_METHOD_FUNC(file_s_extattr_set_link), -1);

    rb_define_method(rb_cFile, "extattr_delete", RUBY_METHOD_FUNC(file_extattr_delete), -1);
    rb_define_singleton_method(rb_cFile, "extattr_delete", RUBY_METHOD_FUNC(file_s_extattr_delete), -1);
    rb_define_singleton_method(rb_cFile, "extattr_delete!", RUBY_METHOD_FUNC(file_s_extattr_delete_link), -1);

    setup();
}
