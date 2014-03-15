#include <sys/types.h>
#include <sys/extattr.h>


static void
extattr_list_name(const char list[], size_t size, VALUE infection_source, void (*func)(void *, VALUE), void *userdata)
{
    // Each list entry consists of a single byte containing the length of
    // the attribute name, followed by the attribute name.
    // The attribute name is not terminated by ASCII 0 (nul).
    const char *ptr = list;
    const char *end = list + size;

    while (ptr < end) {
        size_t len = (uint8_t)*ptr ++;
        if (ptr + len > end) { return; }
        VALUE v = rb_str_new(ptr, len);
        OBJ_INFECT(v, infection_source);
        func(userdata, v);
        ptr += len;
    }
}

static int
get_extattr_list_size(int (*extattr_list)(), intptr_t d, int namespace1)
{
    int size = extattr_list(d, namespace1, NULL, 0);
    if (size < 0) { rb_sys_fail("extattr_list call error"); }
    return size;
}

static VALUE
extattr_list_common(int (*extattr_list)(), intptr_t d, VALUE infection_source, int namespace1)
{
    size_t size = get_extattr_list_size(extattr_list, d, namespace1);
    VALUE buf;
    char *ptr = ALLOCV(buf, size);

    ssize_t size1 = extattr_list(d, namespace1, ptr, size);
    if (size1 < 0) { rb_sys_fail("extattr_list call error"); }

    VALUE list = Qnil;
    if (rb_block_given_p()) {
        extattr_list_name(ptr, size1, infection_source,
                          (void (*)(void *, VALUE))rb_yield_values, (void *)(1));
    } else {
        list = rb_ary_new();
        OBJ_INFECT(list, infection_source);
        extattr_list_name(ptr, size1, infection_source,
                          (void (*)(void *, VALUE))rb_ary_push, (void *)list);
    }
    ALLOCV_END(buf);

    return list;
}

static VALUE
file_extattr_list_main(VALUE file, int fd, int namespace1)
{
    return extattr_list_common(extattr_list_fd, fd, file, namespace1);
}

static VALUE
file_s_extattr_list_main(VALUE path, int namespace1)
{
    return extattr_list_common(extattr_list_file, (intptr_t)StringValueCStr(path), path, namespace1);
}

static VALUE
file_s_extattr_list_link_main(VALUE path, int namespace1)
{
    return extattr_list_common(extattr_list_link, (intptr_t)StringValueCStr(path), path, namespace1);
}


static VALUE
extattr_size_common(int (*extattr_get)(), intptr_t d, int namespace1, VALUE name)
{
    ssize_t size = extattr_get(d, namespace1, RSTRING_PTR(name), NULL, 0);
    if (size < 0) { rb_sys_fail("extattr_get call error"); }
    return SIZET2NUM(size);
}

static VALUE
file_extattr_size_main(VALUE file, int fd, int namespace1, VALUE name)
{
    return extattr_size_common(extattr_get_fd, fd, namespace1, name);
}

static VALUE
file_s_extattr_size_main(VALUE path, int namespace1, VALUE name)
{
    return extattr_size_common(extattr_get_file, (intptr_t)StringValueCStr(path), namespace1, name);
}

static VALUE
file_s_extattr_size_link_main(VALUE path, int namespace1, VALUE name)
{
    return extattr_size_common(extattr_get_link, (intptr_t)StringValueCStr(path), namespace1, name);
}


static VALUE
extattr_get_common(int (*extattr_get)(), intptr_t d, VALUE path, int namespace1, VALUE name)
{
    ssize_t size = extattr_get(d, namespace1, RSTRING_PTR(name), NULL, 0);
    if (size < 0) { rb_sys_fail(StringValueCStr(path)); }
    VALUE buf = rb_str_buf_new(size);
    size = extattr_get(d, namespace1, RSTRING_PTR(name), RSTRING_PTR(buf), size);
    if (size < 0) { rb_sys_fail(StringValueCStr(path)); }
    rb_str_set_len(buf, size);
    return buf;
}

static VALUE
file_extattr_get_main(VALUE file, int fd, int namespace1, VALUE name)
{
    return extattr_get_common(extattr_get_fd, fd, RFILE(file)->fptr->pathv, namespace1, name);
}

static VALUE
file_s_extattr_get_main(VALUE path, int namespace1, VALUE name)
{
    return extattr_get_common(extattr_get_file, (intptr_t)StringValueCStr(path), path, namespace1, name);
}

static VALUE
file_s_extattr_get_link_main(VALUE path, int namespace1, VALUE name)
{
    return extattr_get_common(extattr_get_link, (intptr_t)StringValueCStr(path), path, namespace1, name);
}


static VALUE
extattr_set_common(int (*extattr_set)(), intptr_t d, int namespace1, VALUE name, VALUE data)
{
    int status = extattr_set(d, namespace1, RSTRING_PTR(name), RSTRING_PTR(data), RSTRING_LEN(data));
    if (status < 0) { rb_sys_fail("extattr_set call error"); }
    return Qnil;
}

static VALUE
file_extattr_set_main(VALUE file, int fd, int namespace1, VALUE name, VALUE data)
{
    return extattr_set_common(extattr_set_fd, fd, namespace1, name, data);
}

static VALUE
file_s_extattr_set_main(VALUE path, int namespace1, VALUE name, VALUE data)
{
    return extattr_set_common(extattr_set_file, (intptr_t)StringValueCStr(path), namespace1, name, data);
}

static VALUE
file_s_extattr_set_link_main(VALUE path, int namespace1, VALUE name, VALUE data)
{
    return extattr_set_common(extattr_set_link, (intptr_t)StringValueCStr(path), namespace1, name, data);
}


static VALUE
extattr_delete_common(int (*extattr_delete)(), intptr_t d, int namespace1, VALUE name)
{
    int status = extattr_delete(d, namespace1, RSTRING_PTR(name), NULL, 0);
    if (status < 0) { rb_sys_fail("extattr_delete call error"); }
    return Qnil;
}

static VALUE
file_extattr_delete_main(VALUE file, int fd, int namespace1, VALUE name)
{
    return extattr_delete_common(extattr_delete_fd, fd, namespace1, name);
}

static VALUE
file_s_extattr_delete_main(VALUE path, int namespace1, VALUE name)
{
    return extattr_delete_common(extattr_delete_file, (intptr_t)StringValueCStr(path), namespace1, name);
}

static VALUE
file_s_extattr_delete_link_main(VALUE path, int namespace1, VALUE name)
{
    return extattr_delete_common(extattr_delete_link, (intptr_t)StringValueCStr(path), namespace1, name);
}


static void
ext_init_implement(void)
{
    rb_define_const(rb_cFile, "EXTATTR_IMPLEMANT", rb_str_freeze(rb_str_new_cstr("extattr")));
}
