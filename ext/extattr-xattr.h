#include <sys/types.h>
#if HAVE_ATTR_XATTR_H
#   include <attr/xattr.h>
#else
#   include <sys/xattr.h>
#endif

enum {
    EXTATTR_NAMESPACE_USER,
    EXTATTR_NAMESPACE_SYSTEM,
    EXTATTR_NAMESPACE_TRUSTED,
    EXTATTR_NAMESPACE_SECURITY,
};


static VALUE NAMESPACE_USER_PREFIX, NAMESPACE_SYSTEM_PREFIX;


static VALUE
xattr_name(int namespace1, VALUE name)
{
    switch (namespace1) {
    case EXTATTR_NAMESPACE_USER:
        return rb_str_plus(NAMESPACE_USER_PREFIX, name);
    case EXTATTR_NAMESPACE_SYSTEM:
        return rb_str_plus(NAMESPACE_SYSTEM_PREFIX, name);
    default:
        rb_raise(rb_eRuntimeError, "namespace1 error");
        return Qnil;
    }
}


static inline void
extattr_list_name(const char *ptr, size_t size, VALUE infection_source, int namespace1, VALUE (*func)(void *, VALUE), void *user)
{
    const char *end = ptr + size;
    while (ptr < end) {
        int len = strlen(ptr);
        // if (len > (end - ptr)) { ... } // TODO
        if (namespace1 == EXTATTR_NAMESPACE_USER && len > 5 && strncmp(ptr, "user.", 5) == 0) {
            ptr += 5;
        } else if (namespace1 == EXTATTR_NAMESPACE_SYSTEM && len > 7 && strncmp(ptr, "system.", 7) == 0) {
            ptr += 7;
        } else {
            ptr += len + 1;
            continue;
        }

        VALUE name = rb_str_new_cstr(ptr);
        OBJ_INFECT(name, infection_source);
        func(user, name);
        ptr += RSTRING_LEN(name) + 1; // 最後の『+1』は、ヌルバイトの分。
    }
}

static VALUE
extattr_list_common(ssize_t (*func)(), void *d, VALUE infection_source, int namespace1)
{
    ssize_t size = 65536;
    VALUE buf = rb_str_buf_new(size);
    char *ptr = RSTRING_PTR(buf);
    size = func(d, ptr, size);
    if (size < 0) { rb_sys_fail("listxattr call error"); }

    if (rb_block_given_p()) {
        extattr_list_name(ptr, size, infection_source, namespace1,
                          (VALUE (*)(void *, VALUE))rb_yield_values,
                          (void *)1);
        return Qnil;
    } else {
        VALUE list = rb_ary_new();
        OBJ_INFECT(list, infection_source);
        extattr_list_name(ptr, size, infection_source, namespace1,
                          (VALUE (*)(void *, VALUE))rb_ary_push,
                          (void *)list);
        return list;
    }
}

static VALUE
file_extattr_list_main(VALUE file, int fd, int namespace1)
{
    return extattr_list_common(flistxattr, (void *)fd,
                               file, namespace1);
}

static VALUE
file_s_extattr_list_main(VALUE path, int namespace1)
{
    return extattr_list_common(listxattr, StringValueCStr(path),
                               path, namespace1);
}

static VALUE
file_s_extattr_list_link_main(VALUE path, int namespace1)
{
    return extattr_list_common(llistxattr, StringValueCStr(path),
                               path, namespace1);
}


static VALUE
extattr_size_common(ssize_t (*func)(), void *d, int namespace1, VALUE name)
{
    name = xattr_name(namespace1, name);
    ssize_t size = func(d, StringValueCStr(name), NULL, 0);
    if (size < 0) { rb_sys_fail("getxattr call error"); }
    return SSIZET2NUM(size);
}

static VALUE
file_extattr_size_main(VALUE file, int fd, int namespace1, VALUE name)
{
    return extattr_size_common(fgetxattr, (void *)fd, namespace1, name);
}

static VALUE
file_s_extattr_size_main(VALUE path, int namespace1, VALUE name)
{
    return extattr_size_common(getxattr, StringValueCStr(path), namespace1, name);
}

static VALUE
file_s_extattr_size_link_main(VALUE path, int namespace1, VALUE name)
{
    return extattr_size_common(lgetxattr, StringValueCStr(path), namespace1, name);
}


static VALUE
extattr_get_common(ssize_t (*func)(), void *d, int namespace1, VALUE name)
{
    name = xattr_name(namespace1, name);
    ssize_t size = 65536;
    VALUE buf = rb_str_buf_new(size);
    char *ptr = RSTRING_PTR(buf);
    size = func(d, StringValueCStr(name), ptr, size);
    if (size < 0) { rb_sys_fail("getxattr call error"); }
    rb_str_set_len(buf, size);
    return buf;
}

static VALUE
file_extattr_get_main(VALUE file, int fd, int namespace1, VALUE name)
{
    return extattr_get_common(fgetxattr, (void *)fd, namespace1, name);
}

static VALUE
file_s_extattr_get_main(VALUE path, int namespace1, VALUE name)
{
    return extattr_get_common(getxattr, StringValueCStr(path), namespace1, name);
}

static VALUE
file_s_extattr_get_link_main(VALUE path, int namespace1, VALUE name)
{
    return extattr_get_common(lgetxattr, StringValueCStr(path), namespace1, name);
}


static VALUE
extattr_set_common(int (*func)(), void *d, int namespace1, VALUE name, VALUE data)
{
    name = xattr_name(namespace1, name);
    int status = func(d, StringValueCStr(name), RSTRING_PTR(data), RSTRING_LEN(data), 0);
    if (status < 0) { rb_sys_fail("setxattr call error"); }
    return Qnil;
}

static VALUE
file_extattr_set_main(VALUE file, int fd, int namespace1, VALUE name, VALUE data)
{
    return extattr_set_common(fsetxattr, (void *)fd, namespace1, name, data);
}

static VALUE
file_s_extattr_set_main(VALUE path, int namespace1, VALUE name, VALUE data)
{
    return extattr_set_common(setxattr, StringValueCStr(path), namespace1, name, data);
}

static VALUE
file_s_extattr_set_link_main(VALUE path, int namespace1, VALUE name, VALUE data)
{
    return extattr_set_common(lsetxattr, StringValueCStr(path), namespace1, name, data);
}


static VALUE
extattr_delete_common(int (*func)(), void *d, int namespace1, VALUE name)
{
    name = xattr_name(namespace1, name);
    int status = func(d, StringValueCStr(name));
    if (status < 0) { rb_sys_fail("removexattr call error"); }
    return Qnil;
}

static VALUE
file_extattr_delete_main(VALUE file, int fd, int namespace1, VALUE name)
{
    return extattr_delete_common(fremovexattr, (void *)fd, namespace1, name);
}

static VALUE
file_s_extattr_delete_main(VALUE path, int namespace1, VALUE name)
{
    return extattr_delete_common(removexattr, StringValueCStr(path), namespace1, name);
}

static VALUE
file_s_extattr_delete_link_main(VALUE path, int namespace1, VALUE name)
{
    return extattr_delete_common(lremovexattr, StringValueCStr(path), namespace1, name);
}


static void
ext_init_implement(void)
{
    NAMESPACE_USER_PREFIX = rb_str_new_cstr("user.");
    NAMESPACE_SYSTEM_PREFIX = rb_str_new_cstr("system.");

    rb_gc_register_mark_object(NAMESPACE_USER_PREFIX);
    rb_gc_register_mark_object(NAMESPACE_SYSTEM_PREFIX);

    rb_define_const(rb_cFile, "EXTATTR_IMPLEMANT", rb_str_freeze(rb_str_new_cstr("xattr")));
}
