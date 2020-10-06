/* encoding:utf-8 */

/*
 * extattr.c
 * AUTHOR:: dearblue <dearblue@users.osdn.me>
 * LICENSE:: 2-clause BSD License
 * PROJECT-PAGE:: https://github.com/dearblue/ruby-extattr
 */

#include <ruby.h>
#include <ruby/io.h>
#include <ruby/intern.h>
#include <ruby/version.h>


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
static void extattr_init_implement(void);

#if RUBY_API_VERSION_CODE >= 20700
# define rb_obj_infect(OBJ, SRC) ((void)0)
#endif

#define RDOC(...)

#define ELEMENTOF(V) (sizeof(V) / sizeof((V)[0]))

#define LOGF(FORMAT, ...) { fprintf(stderr, "%s:%d:%s: " FORMAT "\n", __FILE__, __LINE__, __func__, __VA_ARGS__); }

static VALUE mExtAttr, mConstants;

static ID id_downcase;
static ID id_to_path;


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

static VALUE
aux_to_path(VALUE path)
{
    if (rb_respond_to(path, id_to_path)) {
        VALUE args[] = { path };
        path = rb_funcall2(path, id_to_path, 1, args);
        rb_check_type(path, RUBY_T_STRING);
    } else {
        path = StringValue(path);
    }

    return path;
}

static void
aux_sys_fail(VALUE filesrc, const char *funcname)
{
    VALUE tmp;
    filesrc = aux_to_path(filesrc);
    if (funcname) {
        tmp = rb_sprintf("%s (%s error)", StringValueCStr(filesrc), funcname);
    } else {
        tmp = rb_sprintf("%s", StringValueCStr(filesrc));
    }
    rb_sys_fail(StringValueCStr(tmp));
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


static VALUE
aux_should_be_string(VALUE obj)
{
    rb_check_type(obj, RUBY_T_STRING);
    return obj;
}

static int
convert_namespace_int(VALUE namespace)
{
    int n = NUM2INT(namespace);

    if (n == EXTATTR_NAMESPACE_USER ||
        n == EXTATTR_NAMESPACE_SYSTEM) {

        return n;
    }

    rb_raise(rb_eArgError,
            "wrong namespace value - #<%s:%p>",
            rb_obj_classname(namespace), (void *)namespace);
}

static int
convert_namespace_str(VALUE namespace)
{
    static const char ns_user[] = "user";
    static const int ns_user_len = sizeof(ns_user) / sizeof(ns_user[0]) - 1;
    static const char ns_system[] = "system";
    static const int ns_system_len = sizeof(ns_system) / sizeof(ns_system[0]) - 1;

    VALUE namespace1 = rb_funcall2(namespace, id_downcase, 0, 0);
    rb_check_type(namespace1, RUBY_T_STRING);
    const char *p;
    int len;
    RSTRING_GETMEM(namespace1, p, len);

    if (len == ns_user_len && memcmp(p, ns_user, ns_user_len) == 0) {
        return EXTATTR_NAMESPACE_USER;
    } else if (len == ns_system_len && memcmp(p, ns_system, ns_system_len) == 0) {
        return EXTATTR_NAMESPACE_SYSTEM;
    } else {
        rb_raise(rb_eArgError,
                "wrong namespace - %s (expected to user or system)",
                StringValueCStr(namespace));
    }
}

static int
conv_namespace(VALUE namespace)
{
    if (rb_obj_is_kind_of(namespace, rb_cNumeric)) {
        return convert_namespace_int(namespace);
    } else if (rb_obj_is_kind_of(namespace, rb_cString) ||
               rb_obj_is_kind_of(namespace, rb_cSymbol)) {
        return convert_namespace_str(rb_String(namespace));
    } else {
        rb_raise(rb_eArgError,
                "wrong namespace object - %p",
                (void *)namespace);
    }
}

#if RUBY_API_VERSION_CODE >= 20700
static void ext_check_file_security(VALUE file, VALUE name, VALUE data) { }
static void ext_check_path_security(VALUE path, VALUE name, VALUE data) { }
#else
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
#endif


/*
 * call-seq:
 *  list(path, namespace) -> names array
 *  list(path, namespace) { |name| ... } -> nil
 */
static VALUE
ext_s_list(VALUE mod, VALUE path, VALUE namespace)
{
    if (rb_obj_is_kind_of(path, rb_cFile)) {
        ext_check_file_security(path, Qnil, Qnil);
        return file_extattr_list_main(path, file2fd(path),
                conv_namespace(namespace));
    } else {
        ext_check_path_security(path, Qnil, Qnil);
        return file_s_extattr_list_main(aux_to_path(path),
                conv_namespace(namespace));
    }
}

/*
 * call-seq:
 *  extattr_list!(path, namespace) -> names array
 *  extattr_list!(path, namespace) { |name| ... } -> nil
 */
static VALUE
ext_s_list_link(VALUE mod, VALUE path, VALUE namespace)
{
    if (rb_obj_is_kind_of(path, rb_cFile)) {
        ext_check_file_security(path, Qnil, Qnil);
        return file_extattr_list_main(path, file2fd(path),
                conv_namespace(namespace));
    } else {
        ext_check_path_security(path, Qnil, Qnil);
        return file_s_extattr_list_link_main(aux_to_path(path),
                conv_namespace(namespace));
    }
}


/*
 * call-seq:
 *  size(path, namespace, name) -> size
 */
static VALUE
ext_s_size(VALUE mod, VALUE path, VALUE namespace, VALUE name)
{
    if (rb_obj_is_kind_of(path, rb_cFile)) {
        ext_check_file_security(path, name, Qnil);
        return file_extattr_size_main(path, file2fd(path), conv_namespace(namespace),
                aux_should_be_string(name));
    } else {
        ext_check_path_security(path, name, Qnil);
        return file_s_extattr_size_main(aux_to_path(path), conv_namespace(namespace),
                aux_should_be_string(name));
    }
}

/*
 * call-seq:
 *  size!(path, namespace, name) -> size
 */
static VALUE
ext_s_size_link(VALUE mod, VALUE path, VALUE namespace, VALUE name)
{
    if (rb_obj_is_kind_of(path, rb_cFile)) {
        ext_check_file_security(path, name, Qnil);
        return file_extattr_size_main(path, file2fd(path), conv_namespace(namespace),
                aux_should_be_string(name));
    } else {
        ext_check_path_security(path, name, Qnil);
        return file_s_extattr_size_link_main(aux_to_path(path), conv_namespace(namespace),
                aux_should_be_string(name));
    }
}


/*
 * call-seq:
 *  get(path, namespace, name) -> data
 */
static VALUE
ext_s_get(VALUE mod, VALUE path, VALUE namespace, VALUE name)
{
    VALUE v;
    if (rb_obj_is_kind_of(path, rb_cFile)) {
        ext_check_file_security(path, name, Qnil);
        v = file_extattr_get_main(path, file2fd(path), conv_namespace(namespace),
                aux_should_be_string(name));
    } else {
        ext_check_path_security(path, name, Qnil);
        v = file_s_extattr_get_main(aux_to_path(path), conv_namespace(namespace),
                aux_should_be_string(name));
    }

    rb_obj_infect(v, path);
    return v;
}

/*
 * call-seq:
 *  get!(path, namespace, name) -> data
 */
static VALUE
ext_s_get_link(VALUE mod, VALUE path, VALUE namespace, VALUE name)
{
    VALUE v;
    if (rb_obj_is_kind_of(path, rb_cFile)) {
        ext_check_file_security(path, name, Qnil);
        v = file_extattr_get_main(path, file2fd(path), conv_namespace(namespace),
                aux_should_be_string(name));
    } else {
        ext_check_path_security(path, name, Qnil);
        v = file_s_extattr_get_link_main(aux_to_path(path), conv_namespace(namespace),
                aux_should_be_string(name));
    }

    rb_obj_infect(v, path);
    return v;
}


/*
 * call-seq:
 *  set(path, namespace, name, data) -> nil
 */
static VALUE
ext_s_set(VALUE mod, VALUE path, VALUE namespace, VALUE name, VALUE data)
{
    if (rb_obj_is_kind_of(path, rb_cFile)) {
        ext_check_file_security(path, name, Qnil);
        return file_extattr_set_main(path, file2fd(path),
                conv_namespace(namespace),
                aux_should_be_string(name), aux_should_be_string(data));
    } else {
        ext_check_path_security(path, name, Qnil);
        return file_s_extattr_set_main(aux_to_path(path),
                conv_namespace(namespace),
                aux_should_be_string(name), aux_should_be_string(data));
    }
}

/*
 * call-seq:
 *  set!(path, namespace, name, data) -> nil
 */
static VALUE
ext_s_set_link(VALUE mod, VALUE path, VALUE namespace, VALUE name, VALUE data)
{
    if (rb_obj_is_kind_of(path, rb_cFile)) {
        ext_check_file_security(path, name, Qnil);
        return file_extattr_set_main(path, file2fd(path),
                conv_namespace(namespace),
                aux_should_be_string(name), aux_should_be_string(data));
    } else {
        ext_check_path_security(path, name, Qnil);
        return file_s_extattr_set_link_main(aux_to_path(path),
                conv_namespace(namespace),
                aux_should_be_string(name), aux_should_be_string(data));
    }
}


/*
 * call-seq:
 *  delete(path, namespace, name) -> nil
 */
static VALUE
ext_s_delete(VALUE mod, VALUE path, VALUE namespace, VALUE name)
{
    if (rb_obj_is_kind_of(path, rb_cFile)) {
        ext_check_file_security(path, name, Qnil);
        return file_extattr_delete_main(path, file2fd(path), conv_namespace(namespace),
                aux_should_be_string(name));
    } else {
        ext_check_path_security(path, name, Qnil);
        return file_s_extattr_delete_main(aux_to_path(path), conv_namespace(namespace),
                aux_should_be_string(name));
    }
}

/*
 * call-seq:
 *  delete!(path, namespace, name) -> nil
 */
static VALUE
ext_s_delete_link(VALUE mod, VALUE path, VALUE namespace, VALUE name)
{
    if (rb_obj_is_kind_of(path, rb_cFile)) {
        ext_check_file_security(path, name, Qnil);
        return file_extattr_delete_main(path, file2fd(path), conv_namespace(namespace),
                aux_should_be_string(name));
    } else {
        ext_check_path_security(path, name, Qnil);
        return file_s_extattr_delete_link_main(aux_to_path(path), conv_namespace(namespace),
                aux_should_be_string(name));
    }
}


void
Init_extattr(void)
{
    id_downcase = rb_intern("downcase");
    id_to_path = rb_intern("to_path");

    mExtAttr = rb_define_module("ExtAttr");
    mConstants = rb_define_module_under(mExtAttr, "Constants");
    rb_include_module(mExtAttr, mConstants);

    rb_define_const(mConstants, "NAMESPACE_USER", INT2NUM(EXTATTR_NAMESPACE_USER));
    rb_define_const(mConstants, "NAMESPACE_SYSTEM", INT2NUM(EXTATTR_NAMESPACE_SYSTEM));

    rb_define_singleton_method(mExtAttr, "list", RUBY_METHOD_FUNC(ext_s_list), 2);
    rb_define_singleton_method(mExtAttr, "list!", RUBY_METHOD_FUNC(ext_s_list_link), 2);
    rb_define_singleton_method(mExtAttr, "size", RUBY_METHOD_FUNC(ext_s_size), 3);
    rb_define_singleton_method(mExtAttr, "size!", RUBY_METHOD_FUNC(ext_s_size_link), 3);
    rb_define_singleton_method(mExtAttr, "get", RUBY_METHOD_FUNC(ext_s_get), 3);
    rb_define_singleton_method(mExtAttr, "get!", RUBY_METHOD_FUNC(ext_s_get_link), 3);
    rb_define_singleton_method(mExtAttr, "set", RUBY_METHOD_FUNC(ext_s_set), 4);
    rb_define_singleton_method(mExtAttr, "set!", RUBY_METHOD_FUNC(ext_s_set_link), 4);
    rb_define_singleton_method(mExtAttr, "delete", RUBY_METHOD_FUNC(ext_s_delete), 3);
    rb_define_singleton_method(mExtAttr, "delete!", RUBY_METHOD_FUNC(ext_s_delete_link), 3);

    extattr_init_implement();
}
