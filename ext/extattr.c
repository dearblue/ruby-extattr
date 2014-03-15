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


static VALUE file_extattr_list_main(VALUE file, int fd, int namespace1);
static VALUE file_extattr_size_main(VALUE file, int fd, int namespace1, VALUE name);
static VALUE file_extattr_get_main(VALUE file, int fd, int namespace1, VALUE name);
static VALUE file_extattr_set_main(VALUE file, int fd, int namespace1, VALUE name, VALUE data);
static VALUE file_extattr_delete_main(VALUE file, int fd, int namespace1, VALUE name);
static VALUE file_s_extattr_list_main(VALUE path, int namespace1);
static VALUE file_s_extattr_list_link_main(VALUE path, int namespace1);
static VALUE file_s_extattr_size_main(VALUE path, int namespace1, VALUE name);
static VALUE file_s_extattr_size_link_main(VALUE path, int namespace1, VALUE name);
static VALUE file_s_extattr_get_main(VALUE path, int namespace1, VALUE name);
static VALUE file_s_extattr_get_link_main(VALUE path, int namespace1, VALUE name);
static VALUE file_s_extattr_set_main(VALUE path, int namespace1, VALUE name, VALUE data);
static VALUE file_s_extattr_set_link_main(VALUE path, int namespace1, VALUE name, VALUE data);
static VALUE file_s_extattr_delete_main(VALUE path, int namespace1, VALUE name);
static VALUE file_s_extattr_delete_link_main(VALUE path, int namespace1, VALUE name);

// Init_extattr から呼び出される、環境ごとの初期設定。
static void ext_init_implement(void);


#define RDOC(...)

#define ELEMENTOF(V) (sizeof(V) / sizeof((V)[0]))

#define LOGF(FORMAT, ...) { fprintf(stderr, "%s:%d:%s: " FORMAT "\n", __FILE__, __LINE__, __func__, __VA_ARGS__); }


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

static inline void
ext_error_extattr(int err, VALUE filepath, VALUE attrname)
{
    errno = err;
    if (NIL_P(filepath)) {
        filepath = rb_str_new_cstr("<?>");
    } else {
        filepath = rb_get_path_no_checksafe(filepath);
    }
    VALUE mesg = rb_sprintf("%s [%s]", StringValueCStr(filepath), StringValueCStr(attrname));
    rb_sys_fail(StringValueCStr(mesg));
}

static inline void
ext_error_namespace(VALUE path, int namespace1)
{
    ext_error_extattr(EPERM, path, rb_sprintf("namespace=%d", namespace1));
}


#if defined(HAVE_SYS_EXTATTR_H)
#   include "extattr-extattr.h"
#elif defined(HAVE_WINNT_H)
#   include "extattr-windows.h"
#elif defined(HAVE_ATTR_XATTR_H) || defined(HAVE_SYS_XATTR_H)
#   include "extattr-xattr.h"
#else
#   error ruby-extattr not supported on your system
#endif


static VALUE SYMnamespace;

static int
ext_get_namespace(VALUE opts)
{
    return NUM2INT(hash_lookup(opts, SYMnamespace, INT2NUM(EXTATTR_NAMESPACE_USER)));
}

static void
ext_check_file_security(VALUE file, VALUE name, VALUE data)
{
    /*
     * - file が汚染されていない場合、他のオブジェクトも汚染されていてはならない。
     * - file が汚染されている場合、他のオブジェクトの汚染に左右されない。
     * - $SAFE が4以上の場合、非汚染オブジェクトがあれば失敗する。
     *
     *      file name data | list    get     set     delete
     *      -    -    -    | yes     yes     yes     yes
     *      -    -    T    |                 no
     *      -    T    -    |         no      no      no
     *      -    T    T    |                 no
     *      T    -    -    | yes     yes     yes     yes
     *      T    -    T    |                 yes
     *      T    T    -    |         yes     yes     yes
     *      T    T    T    |                 yes
     */

    int safe = rb_safe_level();

    // $SAFE < 1 であれば、常に成功する
    if (safe < 1) {
        return;
    }

    // 0 < $SAFE < 4 であれば、file が汚染されていれば常に成功、
    // そうでなければ name と data は非汚染状態でなければならない
    if (safe < 4) {
        if (OBJ_TAINTED(file)) {
            return;
        }
        if (OBJ_TAINTED(name) || OBJ_TAINTED(data)) {
            rb_insecure_operation();
        }
        return;
    }

    // $SAFE は 4以上。すべてのオブジェクトが汚染されていなければならない
    if (!OBJ_TAINTED(file) ||
        (!NIL_P(name) && !OBJ_TAINTED(name)) ||
        (!NIL_P(data) && !OBJ_TAINTED(data))) {

        rb_insecure_operation();
    }
}

static void
ext_check_path_security(VALUE path, VALUE name, VALUE data)
{
    /*
     * - すべてのオブジェクトが汚染されていない状態でなければならない。
     * - $SAFE が4以上の場合、常に失敗する。
     *
     *      path name data | list    get     set     delete
     *      -    -    -    | yes     yes     yes     yes
     *      -    -    T    |                 no
     *      -    T    -    |         no      no      no
     *      -    T    T    |                 no
     *      T    -    -    | no      no      no      no
     *      T    -    T    |                 no
     *      T    T    -    |         no      no      no
     *      T    T    T    |                 no
     */

    int safe = rb_safe_level();
    if (safe < 1) { return; }
    if (safe < 4) {
        if (OBJ_TAINTED(path) || OBJ_TAINTED(name) || OBJ_TAINTED(data)) {
            rb_insecure_operation();
        }
    } else {
        rb_insecure_operation();
    }
}


/*
 * call-seq:
 *  extattr_list(namespace: File::EXTATTR_NAMESPACE_USER) -> names array
 *  extattr_list(namespace: File::EXTATTR_NAMESPACE_USER) { |name| ... } -> nil
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
    ext_check_file_security(file, Qnil, Qnil);
    return file_extattr_list_main(file, file2fd(file),
                                  ext_get_namespace(opts));
}

/*
 * call-seq:
 *  extattr_list(path, namespace: File::EXTATTR_NAMESPACE_USER) -> names array
 *  extattr_list(path, namespace: File::EXTATTR_NAMESPACE_USER) { |name| ... } -> nil
 *
 * ファイル名を指定すること以外は File#extattr_list と同じです。
 */
static VALUE
file_s_extattr_list(int argc, VALUE argv[], VALUE file)
{
    VALUE path, opts = Qnil;
    rb_scan_args(argc, argv, "1:", &path, &opts);
    ext_check_path_security(path, Qnil, Qnil);
    return file_s_extattr_list_main(StringValue(path),
                                    ext_get_namespace(opts));
}

/*
 * call-seq:
 *  extattr_list!(path, namespace: File::EXTATTR_NAMESPACE_USER) -> names array
 *  extattr_list!(path, namespace: File::EXTATTR_NAMESPACE_USER) { |name| ... } -> nil
 *
 * シンボリックリンクに対する操作という以外は、File.extattr_list と同じです。
 */
static VALUE
file_s_extattr_list_link(int argc, VALUE argv[], VALUE file)
{
    VALUE path, opts = Qnil;
    rb_scan_args(argc, argv, "1:", &path, &opts);
    ext_check_path_security(path, Qnil, Qnil);
    return file_s_extattr_list_link_main(StringValue(path),
                                         ext_get_namespace(opts));
}


/*
 * call-seq:
 *  extattr_size(name, namespace: File::EXTATTR_NAMESPACE_USER) -> names array
 *
 * 拡張属性の大きさを取得します。
 */
static VALUE
file_extattr_size(int argc, VALUE argv[], VALUE file)
{
    VALUE name, opts = Qnil;
    rb_scan_args(argc, argv, "1:", &name, &opts);
    ext_check_file_security(file, name, Qnil);
    Check_Type(name, RUBY_T_STRING);
    return file_extattr_size_main(file, file2fd(file),
                                  ext_get_namespace(opts),
                                  StringValue(name));
}

/*
 * call-seq:
 *  extattr_size(path, name, namespace: File::EXTATTR_NAMESPACE_USER) -> size
 */
static VALUE
file_s_extattr_size(int argc, VALUE argv[], VALUE file)
{
    VALUE path, name, opts = Qnil;
    rb_scan_args(argc, argv, "2:", &path, &name, &opts);
    ext_check_path_security(path, name, Qnil);
    Check_Type(name, RUBY_T_STRING);
    return file_s_extattr_size_main(StringValue(path),
                                    ext_get_namespace(opts),
                                    StringValue(name));
}

/*
 * call-seq:
 *  extattr_size!(path, name, namespace: File::EXTATTR_NAMESPACE_USER) -> size
 */
static VALUE
file_s_extattr_size_link(int argc, VALUE argv[], VALUE file)
{
    VALUE path, name, opts = Qnil;
    rb_scan_args(argc, argv, "2:", &path, &name, &opts);
    ext_check_path_security(path, name, Qnil);
    Check_Type(name, RUBY_T_STRING);
    return file_s_extattr_size_link_main(StringValue(path),
                                         ext_get_namespace(opts),
                                         StringValue(name));
}


/*
 * call-seq:
 *  extattr_get(name, namespace: File::EXTATTR_NAMESPACE_USER) -> data
 */
static VALUE
file_extattr_get(int argc, VALUE argv[], VALUE file)
{
    VALUE name, opts = Qnil;
    rb_scan_args(argc, argv, "1:", &name, &opts);
    ext_check_file_security(file, name, Qnil);
    Check_Type(name, RUBY_T_STRING);
    VALUE v = file_extattr_get_main(file, file2fd(file),
                                    ext_get_namespace(opts),
                                    StringValue(name));
    OBJ_INFECT(v, file);
    return v;
}

/*
 * call-seq:
 *  extattr_get(path, name, namespace: File::EXTATTR_NAMESPACE_USER) -> data
 */
static VALUE
file_s_extattr_get(int argc, VALUE argv[], VALUE file)
{
    VALUE path, name, opts = Qnil;
    rb_scan_args(argc, argv, "2:", &path, &name, &opts);
    ext_check_path_security(path, name, Qnil);
    Check_Type(name, RUBY_T_STRING);
    VALUE v = file_s_extattr_get_main(StringValue(path),
                                      ext_get_namespace(opts),
                                      StringValue(name));
    OBJ_INFECT(v, path);
    return v;
}

/*
 * call-seq:
 *  extattr_get!(path, name, namespace: File::EXTATTR_NAMESPACE_USER) -> data
 */
static VALUE
file_s_extattr_get_link(int argc, VALUE argv[], VALUE file)
{
    VALUE path, name, opts = Qnil;
    rb_scan_args(argc, argv, "2:", &path, &name, &opts);
    ext_check_path_security(path, name, Qnil);
    Check_Type(name, RUBY_T_STRING);
    VALUE v = file_s_extattr_get_link_main(StringValue(path),
                                           ext_get_namespace(opts),
                                           StringValue(name));
    OBJ_INFECT(v, path);
    return v;
}


/*
 * call-seq:
 *  extattr_set(name, data, namespace: File::EXTATTR_NAMESPACE_USER) -> nil
 */
static VALUE
file_extattr_set(int argc, VALUE argv[], VALUE file)
{
    VALUE name, data, opts = Qnil;
    rb_scan_args(argc, argv, "2:", &name, &data, &opts);
    ext_check_file_security(file, name, data);
    Check_Type(name, RUBY_T_STRING);
    return file_extattr_set_main(file, file2fd(file),
                                 ext_get_namespace(opts),
                                 StringValue(name),
                                 StringValue(data));
}

/*
 * call-seq:
 *  extattr_set(path, name, data, namespace: File::EXTATTR_NAMESPACE_USER) -> nil
 */
static VALUE
file_s_extattr_set(int argc, VALUE argv[], VALUE file)
{
    VALUE path, name, data, opts = Qnil;
    rb_scan_args(argc, argv, "3:", &path, &name, &data, &opts);
    ext_check_path_security(path, name, data);
    Check_Type(name, RUBY_T_STRING);
    return file_s_extattr_set_main(StringValue(path),
                                   ext_get_namespace(opts),
                                   StringValue(name),
                                   StringValue(data));
}

/*
 * call-seq:
 *  extattr_set!(path, name, data, namespace: File::EXTATTR_NAMESPACE_USER) -> nil
 */
static VALUE
file_s_extattr_set_link(int argc, VALUE argv[], VALUE file)
{
    VALUE path, name, data, opts = Qnil;
    rb_scan_args(argc, argv, "3:", &path, &name, &data, &opts);
    ext_check_path_security(path, name, data);
    Check_Type(name, RUBY_T_STRING);
    return file_s_extattr_set_link_main(StringValue(path),
                                        ext_get_namespace(opts),
                                        StringValue(name),
                                        StringValue(data));
}


/*
 * call-seq:
 *  extattr_delete(name, namespace: File::EXTATTR_NAMESPACE_USER) -> nil
 */
static VALUE
file_extattr_delete(int argc, VALUE argv[], VALUE file)
{
    VALUE name, opts = Qnil;
    rb_scan_args(argc, argv, "1:", &name, &opts);
    ext_check_file_security(file, name, Qnil);
    Check_Type(name, RUBY_T_STRING);
    return file_extattr_delete_main(file, file2fd(file),
                                    ext_get_namespace(opts),
                                    StringValue(name));
}

/*
 * call-seq:
 *  extattr_delete(path, name, namespace: File::EXTATTR_NAMESPACE_USER) -> nil
 */
static VALUE
file_s_extattr_delete(int argc, VALUE argv[], VALUE file)
{
    VALUE path, name, opts = Qnil;
    rb_scan_args(argc, argv, "2:", &path, &name, &opts);
    ext_check_path_security(path, name, Qnil);
    Check_Type(name, RUBY_T_STRING);
    return file_s_extattr_delete_main(StringValue(path),
                                      ext_get_namespace(opts),
                                      StringValue(name));
}

/*
 * call-seq:
 *  extattr_delete!(path, name, namespace: File::EXTATTR_NAMESPACE_USER) -> nil
 */
static VALUE
file_s_extattr_delete_link(int argc, VALUE argv[], VALUE file)
{
    VALUE path, name, opts = Qnil;
    rb_scan_args(argc, argv, "2:", &path, &name, &opts);
    ext_check_path_security(path, name, Qnil);
    Check_Type(name, RUBY_T_STRING);
    return file_s_extattr_delete_link_main(StringValue(path),
                                           ext_get_namespace(opts),
                                           StringValue(name));
}


void
Init_extattr(void)
{
    SYMnamespace = ID2SYM(rb_intern_const("namespace"));

    rb_define_const(rb_cFile, "EXTATTR_NAMESPACE_USER", INT2NUM(EXTATTR_NAMESPACE_USER));
    rb_define_const(rb_cFile, "EXTATTR_NAMESPACE_SYSTEM", INT2NUM(EXTATTR_NAMESPACE_SYSTEM));
    rb_define_const(rb_cFile, "EXTATTR_VERSION", rb_str_freeze(rb_str_new_cstr("0.2")));

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

    ext_init_implement();
}
