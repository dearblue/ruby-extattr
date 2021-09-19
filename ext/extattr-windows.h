#include <windows.h>
#include <windowsx.h>
#include <winnt.h>
#include <psapi.h>
#include <ntdef.h>
#define PEXECUTION_STATE PEXECUTION_STATE__FAKE
#include <ddk/ntifs.h>
#include <ddk/winddk.h>
#include <wchar.h>
#include <ruby/win32.h>

/*
 * - TODO:
 *   バッファサイズの確保量が結構大きめ。最適化をするべき。
 * - TODO:
 *   Win9X シリーズだとこのライブラリ読み込みすら出来ない。
 *   メソッド定義だけは行うようにするべき? (実際に呼び出したら NotImplementedError 例外を投げる、とか)
 * - リパースポイントって、ADSはつけられないのかな? 操作方法がわからない。
 */

enum {
    EXTATTR_NAMESPACE_USER = 1,
    EXTATTR_NAMESPACE_SYSTEM = 2,

    /*
     * ADS は普通のファイルのように扱えるから、extattr とみなして扱う場合は容量の制限を設けることにする。
     *
     * ruby-extattr で不十分な巨大なデータを扱う場合は File.open で開くことが出来るので必要であればそちらで。
     *
     * TODO: 最適値を探すべき
     */
    EXT_EXTATTR_ADS_DATAMAX = 65535,
    EXT_EXTATTR_ADS_NAMEMAX = 255,

    EXT_EXTATTR_EA_DATAMAX = 65535,
    EXT_EXTATTR_EA_NAMEMAX = 255,

    EXT_NAMETERMSIZE = 1, // C string termination / '\0'
};


static rb_encoding *ENCutf8p;
static VALUE ENCutf8;
static rb_encoding *ENCutf16lep;
static VALUE ENCutf16le;


static VALUE
str2wcs(VALUE str)
{
    str = rb_str_encode(str, ENCutf16le, 0, Qnil);
    rb_str_buf_cat(str, "\0", 1);
    return str;
}

static const wchar_t *
str2wpath(VALUE *str)
{
    VALUE tmp = (*str = str2wcs(*str));
    wchar_t *p = (wchar_t *)RSTRING_PTR(tmp);
    const wchar_t *end = p + RSTRING_LEN(tmp) / 2;
    for (; p < end; p ++) {
        if (*p == L'/') { *p = L'\\'; }
    }
    return (const wchar_t *)RSTRING_PTR(tmp);
}

#define STR2WPATH(str) str2wpath(&str)

static VALUE
wcs2str(const wchar_t str[], size_t size)
{
    if (!str) { return Qnil; }
    VALUE v = rb_str_new((const char *)str, size * sizeof(str[0]));
    rb_enc_associate(v, ENCutf16lep);

    return rb_str_encode(v, ENCutf8, ECONV_INVALID_REPLACE | ECONV_UNDEF_REPLACE, Qnil);
}

static VALUE
wpath2str(const wchar_t path[], size_t size)
{
    VALUE v = wcs2str(path, size);
    if (NIL_P(v)) { return Qnil; }
    char *p = RSTRING_PTR(v);
    const char *end = p + RSTRING_LEN(v);
    for (; p < end; p ++) {
        if (*p == '\\') { *p = '/'; }
    }
    return v;
}

/*
 * ADS 名の先端のコロン『:』と、終端の『:$DATA』を除去して Ruby の文字列を返す。
 */
static VALUE
adsname2str(const wchar_t name[], size_t size)
{
    if (name[0] == L':') {
        name ++;
        size --;
        if (size >= 6 && wcsncmp(name + size - 6, L":$DATA", 6) == 0) {
            size -= 6;
        } else {
            size --;
        }
    }

    if (size > 0) {
        return wpath2str(name, size);
    } else {
        return Qnil;
    }
}

static VALUE
ext_join_adspath(VALUE path, VALUE name)
{
    return rb_str_append(rb_str_plus(path, rb_str_new(":", 1)), name);
}


static void
raise_win32_error(DWORD status, VALUE name)
{
    if (NIL_P(name)) {
        VALUE args[] = { INT2NUM(rb_w32_map_errno(status)), };
        rb_exc_raise(rb_class_new_instance(1, args, rb_eSystemCallError));
    } else {
        // TODO: name がファイルオブジェクトなのであればパス名を取り出す。パス名がない場合は fd を取り出す。

        VALUE args[] = { StringValue(name), INT2NUM(rb_w32_map_errno(status)), };
        rb_exc_raise(rb_class_new_instance(2, args, rb_eSystemCallError));
    }
}

static void
raise_ntstatus_error(NTSTATUS status, VALUE name)
{
    raise_win32_error(RtlNtStatusToDosError(status), name);
}

static void
check_status_error(NTSTATUS status)
{
    if (status != STATUS_SUCCESS) {
        raise_ntstatus_error(status, Qnil);
    }
}


static VALUE
get_filepath(HANDLE file)
{
    DWORD size = 65536; // ファイルの完全パス名の最大バイト数 (UTF-16 換算で32768文字)
    VALUE buf = rb_str_buf_new(size);
    OBJECT_NAME_INFORMATION *info = (void *)RSTRING_PTR(buf);
    NTSTATUS status = NtQueryObject(file, ObjectNameInformation,
                                    info, size, &size);
    check_status_error(status);

    if (wcsnicmp(info->Name.Buffer, L"\\Device\\", 8) == 0) {
        // 先頭の "/Device/" を "//?/" に置き換える
        wcsncpy(info->Name.Buffer + 4, L"\\\\?\\", 4);
        return wpath2str(info->Name.Buffer + 4, info->Name.Length / 2 - 4);
    } else {
        return wpath2str(info->Name.Buffer, info->Name.Length / 2);
    }
}

static uint64_t
get_filesize(HANDLE file)
{
    BY_HANDLE_FILE_INFORMATION info;

    if (!GetFileInformationByHandle(file, &info)) {
        raise_win32_error(GetLastError(), Qnil);
    }

    return ((uint64_t)info.nFileSizeHigh << 32) | info.nFileSizeLow;
}

static VALUE
file_close(HANDLE file)
{
    // rb_ensure から直接呼びたかったけど、呼び出し規約が違うから無理だよね。

    CloseHandle(file);

    return Qnil;
}

typedef VALUE (ext_extattr_ctrl_f)(HANDLE file, VALUE pathsrc, VALUE extname, VALUE extdata);

static VALUE
ext_extattr_ctrl_common_try(VALUE args[])
{
    return ((ext_extattr_ctrl_f *)args[4])((HANDLE)args[0], args[1], args[2], args[3]);
}

static VALUE
ext_extattr_ctrl_fileopen(VALUE path, int flags, int access, int create, VALUE pathsrc, VALUE name, VALUE data, ext_extattr_ctrl_f *func)
{
    HANDLE file = CreateFileW(STR2WPATH(path), access,
                              FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE,
                              NULL, create,
                              FILE_FLAG_BACKUP_SEMANTICS | flags,
                              0);
    if (file == INVALID_HANDLE_VALUE) { raise_win32_error(GetLastError(), pathsrc); }
    VALUE args[] = { (VALUE)file, pathsrc, name, data, (VALUE)func };
    return rb_ensure(ext_extattr_ctrl_common_try, (VALUE)args, file_close, (VALUE)file);
}

struct ext_extattr_ctrl_traits_t
{
    int access;
    int create;
    ext_extattr_ctrl_f *func;
};

struct ext_extattr_ctrl_traits_set_t
{
    struct ext_extattr_ctrl_traits_t user;
    struct ext_extattr_ctrl_traits_t system;
};

static VALUE
ext_extattr_ctrl_ads(HANDLE file, VALUE path, int flags, VALUE pathsrc, VALUE name, VALUE data, const struct ext_extattr_ctrl_traits_t *traits)
{
    if (file) {
        path = get_filepath(file);
    }

    if (!NIL_P(name)) {
        path = ext_join_adspath(path, name);
    }

    return ext_extattr_ctrl_fileopen(path, flags, traits->access, traits->create,
                                     pathsrc, name, data, traits->func);
}

static VALUE
ext_extattr_ctrl_ea(HANDLE file, VALUE path, int flags, VALUE pathsrc, VALUE name, VALUE data, const struct ext_extattr_ctrl_traits_t *traits)
{
    if (file) {
        return traits->func(file, pathsrc, name, data);
    }

    return ext_extattr_ctrl_fileopen(path,
                                     flags, traits->access, traits->create,
                                     pathsrc, name, data, traits->func);
}

static VALUE
ext_extattr_ctrl_common(HANDLE file, VALUE path, int flags, VALUE pathsrc, int namespace1, VALUE name, VALUE data, const struct ext_extattr_ctrl_traits_set_t *traits)
{
    switch (namespace1) {
    case EXTATTR_NAMESPACE_USER:
        return ext_extattr_ctrl_ads(file, path, flags, pathsrc, name, data, &traits->user);
    case EXTATTR_NAMESPACE_SYSTEM:
        return ext_extattr_ctrl_ea(file, path, flags, pathsrc, name, data, &traits->system);
    default:
        ext_error_namespace(pathsrc, namespace1);
        return Qnil;
    }
}

/*
 * extattr_list
 */

static void
extattr_list_ads_name(const char *ptr, VALUE infection_source, void *(*func)(void *, VALUE), void *user)
{
    for (;;) {
        const FILE_STREAM_INFORMATION *info = (const FILE_STREAM_INFORMATION *)ptr;
        VALUE name = adsname2str(info->StreamName, info->StreamNameLength / 2);
        if (!NIL_P(name) && RSTRING_LEN(name) > 0) {
            OBJ_INFECT(name, infection_source);
            func(user, name);
        }
        size_t size = info->NextEntryOffset;
        if (size == 0) { break; }
        ptr += size;
    }
}

typedef void *(extattr_list_ea_push_f)(void *, VALUE);

static VALUE
ext_extattr_list_ads_main(HANDLE file, VALUE pathsrc, VALUE name, VALUE data)
{
    VALUE iostatusblock = rb_str_buf_new(4096);
    size_t size = 65536; // TODO: 最適値を見つける
    VALUE infobuf = rb_str_buf_new(size);
    char *ptr = RSTRING_PTR(infobuf);
    memset(ptr, 0, sizeof(FILE_STREAM_INFORMATION));
    NTSTATUS status = NtQueryInformationFile(file,
                                             (PIO_STATUS_BLOCK)RSTRING_PTR(iostatusblock),
                                             ptr, size,
                                             FileStreamInformation);
    check_status_error(status);

    if (rb_block_given_p()) {
        extattr_list_ads_name(ptr, pathsrc,
                              (void *(*)(void *, VALUE))rb_yield_values,
                              (void *)1);
        return Qnil;
    } else {
        VALUE list = rb_ary_new();
        OBJ_INFECT(list, pathsrc);
        extattr_list_ads_name(ptr, pathsrc,
                              (void *(*)(void *, VALUE))rb_ary_push,
                              (void *)list);
        return list;
    }
}

typedef void *(extattr_list_ea_push_f)(void *, VALUE);

static void
extattr_list_ea_name(HANDLE file, VALUE infection_source, IO_STATUS_BLOCK *iostatusblock, extattr_list_ea_push_f *func, void *funcparam)
{
    size_t bufsize = 4096;
    VALUE infobuf;
    FILE_FULL_EA_INFORMATION *info = (void *)ALLOCV(infobuf, bufsize);
    ULONG index = 1; // one based index
    for (;;) {
        NTSTATUS status = NtQueryEaFile(file, iostatusblock, info, bufsize,
                                        TRUE, NULL, 0, &index, (index == 1 ? TRUE : FALSE));
        if (status == STATUS_BUFFER_OVERFLOW || status == STATUS_BUFFER_TOO_SMALL) {
            bufsize *= 2;
            rb_str_resize(infobuf, bufsize);
            info = (void *)RSTRING_PTR(infobuf);
            continue;
        } else if (status == STATUS_NO_MORE_EAS) {
            break;
        }
        check_status_error(status);
        if (info->EaNameLength > 0) {
            VALUE name = rb_str_new(info->EaName, info->EaNameLength);
            OBJ_INFECT(name, infection_source);
            func(funcparam, name);
        }
        index ++;
    }
    ALLOCV_END(infobuf);
}

static VALUE
ext_extattr_list_ea_main(HANDLE file, VALUE pathsrc, VALUE name, VALUE data)
{
    VALUE iostatusblock_pool = rb_str_buf_new(4096);
    IO_STATUS_BLOCK *iostatusblock = (IO_STATUS_BLOCK *)RSTRING_PTR(iostatusblock_pool);
    FILE_EA_INFORMATION eainfo;
    NTSTATUS status = NtQueryInformationFile(file, iostatusblock,
                                             &eainfo, sizeof(eainfo), FileEaInformation);
    check_status_error(status);

    VALUE namelist;
    extattr_list_ea_push_f *func;
    void *funcparam;
    if (rb_block_given_p()) {
        namelist = Qnil;
        func = (extattr_list_ea_push_f *)rb_yield_values;
        funcparam = (void *)(1); // call as rb_yield_values(1, name)
    } else {
        namelist = rb_ary_new();
        OBJ_INFECT(namelist, pathsrc);
        func = (extattr_list_ea_push_f *)rb_ary_push;
        funcparam = (void *)(namelist); // call as rb_ary_push(namelist, name)
    }

    if (eainfo.EaSize == 0) {
        return namelist;
    }

    extattr_list_ea_name(file, pathsrc, iostatusblock, func, funcparam);

    return namelist;
}

static const struct ext_extattr_ctrl_traits_set_t ext_extattr_ctrl_traits_list = {
    .user = {
        .access = GENERIC_READ,
        .create = OPEN_EXISTING,
        .func = ext_extattr_list_ads_main,
    },
    .system = {
        .access = GENERIC_READ,
        .create = OPEN_EXISTING,
        .func = ext_extattr_list_ea_main,
    },
};

static VALUE
file_extattr_list_main(VALUE file, int fd, int namespace1)
{
    return ext_extattr_ctrl_common((HANDLE)_get_osfhandle(fd), Qnil, 0,
                                   file, namespace1, Qnil, Qnil,
                                   &ext_extattr_ctrl_traits_list);
}

static VALUE
file_s_extattr_list_main(VALUE path, int namespace1)
{
    return ext_extattr_ctrl_common(0, path, 0,
                                   path, namespace1, Qnil, Qnil,
                                   &ext_extattr_ctrl_traits_list);
}

static VALUE
file_s_extattr_list_link_main(VALUE path, int namespace1)
{
    return ext_extattr_ctrl_common(0, path, FILE_FLAG_OPEN_REPARSE_POINT,
                                   path, namespace1, Qnil, Qnil,
                                   &ext_extattr_ctrl_traits_list);
}

/*
 * extattr_size
 */

static VALUE
ext_extattr_size_ads_main(HANDLE file, VALUE pathsrc, VALUE name, VALUE data)
{
    return ULL2NUM(get_filesize(file));
}

static VALUE ext_extattr_get_ea_main(HANDLE file, VALUE pathsrc, VALUE name, VALUE data);

static VALUE
ext_extattr_size_ea_main(HANDLE file, VALUE pathsrc, VALUE name, VALUE data)
{
    VALUE v = ext_extattr_get_ea_main(file, pathsrc, name, data);
    return SIZET2NUM(RSTRING_LEN(v));
}

static const struct ext_extattr_ctrl_traits_set_t ext_extattr_ctrl_traits_size = {
    .user = {
        .access = GENERIC_READ,
        .create = OPEN_EXISTING,
        .func = ext_extattr_size_ads_main,
    },
    .system = {
        .access = GENERIC_READ,
        .create = OPEN_EXISTING,
        .func = ext_extattr_size_ea_main,
    },
};

static VALUE
file_extattr_size_main(VALUE file, int fd, int namespace1, VALUE name)
{
    return ext_extattr_ctrl_common((HANDLE)_get_osfhandle(fd), Qnil, 0,
                                   file, namespace1, name, Qnil,
                                   &ext_extattr_ctrl_traits_size);
}

static VALUE
file_s_extattr_size_main(VALUE path, int namespace1, VALUE name)
{
    return ext_extattr_ctrl_common(0, path, 0,
                                   path, namespace1, name, Qnil,
                                   &ext_extattr_ctrl_traits_size);
}

static VALUE
file_s_extattr_size_link_main(VALUE path, int namespace1, VALUE name)
{
    return ext_extattr_ctrl_common(0, path, FILE_FLAG_OPEN_REPARSE_POINT,
                                   path, namespace1, name, Qnil,
                                   &ext_extattr_ctrl_traits_size);
}

/*
 * extattr_get
 */

static inline size_t
alignunit4(size_t num)
{
    return (num + 3) & ~0x03;
}

static VALUE
ext_extattr_get_ads_main(HANDLE file, VALUE pathsrc, VALUE name, VALUE data)
{
    uint64_t size = get_filesize(file);
    if (size > EXT_EXTATTR_ADS_DATAMAX) { rb_raise(rb_eSystemCallError, "extattr too huge"); }
    VALUE buf = rb_str_buf_new(size);
    DWORD readsize = 0;
    if (!ReadFile(file, RSTRING_PTR(buf), size, &readsize, NULL)) {
        raise_win32_error(GetLastError(), Qnil);
    }
    rb_str_set_len(buf, readsize);
    return buf;
}

static VALUE
ext_extattr_get_ea_main(HANDLE file, VALUE pathsrc, VALUE name, VALUE data)
{
    size_t namesize = RSTRING_LEN(name);
    if (namesize > EXT_EXTATTR_EA_NAMEMAX) {
        ext_error_extattr(ENAMETOOLONG, pathsrc, name);
    }

    size_t bufsize = sizeof(FILE_FULL_EA_INFORMATION) + namesize + EXT_EXTATTR_EA_DATAMAX;
    VALUE info_pool;
    FILE_FULL_EA_INFORMATION *info = ALLOCV(info_pool, bufsize);
    VALUE list_pool;
    FILE_GET_EA_INFORMATION *list = ALLOCV(list_pool, 512);
    list->NextEntryOffset = 0;
    list->EaNameLength = namesize;
    memcpy(list->EaName, RSTRING_PTR(name), namesize);
    list->EaName[namesize] = '\0';

    IO_STATUS_BLOCK iostatusblock;
    memset(&iostatusblock, 0, sizeof(iostatusblock));
    NTSTATUS status = NtQueryEaFile(file, &iostatusblock, info, bufsize,
                                    TRUE, list, 4 + 1 + namesize + 1, NULL, TRUE);
    check_status_error(status);

    if (info->EaValueLength < 1) {
        //ext_error_extattr(ENOATTR, pathsrc, name);
        ext_error_extattr(ENOENT, pathsrc, name);
    }
    VALUE v = rb_str_buf_new(info->EaValueLength);
    rb_str_buf_cat(v, info->EaName + info->EaNameLength + 1, info->EaValueLength);
    return v;
}

static const struct ext_extattr_ctrl_traits_set_t ext_extattr_ctrl_traits_get = {
    .user = {
        .access = GENERIC_READ,
        .create = OPEN_EXISTING,
        .func = ext_extattr_get_ads_main,
    },
    .system = {
        .access = GENERIC_READ,
        .create = OPEN_EXISTING,
        .func = ext_extattr_get_ea_main,
    },
};

static VALUE
file_extattr_get_main(VALUE file, int fd, int namespace1, VALUE name)
{
    return ext_extattr_ctrl_common((HANDLE)_get_osfhandle(fd), Qnil, 0,
                                   file, namespace1, name, Qnil,
                                   &ext_extattr_ctrl_traits_get);
}

static VALUE
file_s_extattr_get_main(VALUE path, int namespace1, VALUE name)
{
    return ext_extattr_ctrl_common(0, path, 0,
                                   path, namespace1, name, Qnil,
                                   &ext_extattr_ctrl_traits_get);
}

static VALUE
file_s_extattr_get_link_main(VALUE path, int namespace1, VALUE name)
{
    return ext_extattr_ctrl_common(0, path, FILE_FLAG_OPEN_REPARSE_POINT,
                                   path, namespace1, name, Qnil,
                                   &ext_extattr_ctrl_traits_get);
}

/*
 * extattr_set
 */

static VALUE
ext_extattr_set_ads_main(HANDLE file, VALUE pathsrc, VALUE name, VALUE data)
{
    uint64_t size = RSTRING_LEN(data);
    if (size > EXT_EXTATTR_ADS_DATAMAX) { ext_error_extattr(E2BIG, pathsrc, name); }
    DWORD wrotesize = size;
    if (!WriteFile(file, RSTRING_PTR(data), size, &wrotesize, NULL)) {
        raise_win32_error(GetLastError(), Qnil);
    }
    return Qnil;
}

static VALUE
ext_extattr_set_ea_main(HANDLE file, VALUE pathsrc, VALUE name, VALUE data)
{
    size_t namesize = RSTRING_LEN(name);
    if (namesize > EXT_EXTATTR_EA_NAMEMAX) { ext_error_extattr(E2BIG, pathsrc, name); }
    size_t datasize = RSTRING_LEN(data);
    if (datasize > EXT_EXTATTR_EA_DATAMAX) { ext_error_extattr(E2BIG, pathsrc, name); }

    size_t infosize = sizeof(FILE_FULL_EA_INFORMATION) + namesize + EXT_NAMETERMSIZE + datasize;
    VALUE info_pool;
    FILE_FULL_EA_INFORMATION *info = ALLOCV(info_pool, infosize);
    memset(info, 0, infosize);
    info->EaNameLength = namesize;
    memcpy(info->EaName, RSTRING_PTR(name), namesize);
    info->EaName[namesize] = '\0';
    info->EaValueLength = datasize;
    memcpy(info->EaName + namesize + EXT_NAMETERMSIZE, RSTRING_PTR(data), datasize);

    IO_STATUS_BLOCK iostatusblock;
    memset(&iostatusblock, 0, sizeof(iostatusblock));
    NTSTATUS status = NtSetEaFile(file, &iostatusblock, info, infosize);
    check_status_error(status);

    return Qnil;
}

static const struct ext_extattr_ctrl_traits_set_t ext_extattr_ctrl_traits_set = {
    .user = {
        .access = GENERIC_WRITE,
        .create = CREATE_ALWAYS,
        .func = ext_extattr_set_ads_main,
    },
    .system = {
        .access = GENERIC_WRITE,
        .create = OPEN_EXISTING,
        .func = ext_extattr_set_ea_main,
    },
};

static VALUE
file_extattr_set_main(VALUE file, int fd, int namespace1, VALUE name, VALUE data)
{
    return ext_extattr_ctrl_common((HANDLE)_get_osfhandle(fd), Qnil, 0,
                                   file, namespace1, name, data,
                                   &ext_extattr_ctrl_traits_set);
}

static VALUE
file_s_extattr_set_main(VALUE path, int namespace1, VALUE name, VALUE data)
{
    return ext_extattr_ctrl_common(0, path, 0,
                                   path, namespace1, name, data,
                                   &ext_extattr_ctrl_traits_set);
}

static VALUE
file_s_extattr_set_link_main(VALUE path, int namespace1, VALUE name, VALUE data)
{
    return ext_extattr_ctrl_common(0, path, FILE_FLAG_OPEN_REPARSE_POINT,
                                   path, namespace1, name, data,
                                   &ext_extattr_ctrl_traits_set);
}

/*
 * extattr_delete
 */

static VALUE
ext_extattr_delete_ads(HANDLE file, VALUE path, VALUE pathsrc, VALUE name)
{
    if (NIL_P(path)) { path = get_filepath(file); }
    VALUE path1 = ext_join_adspath(path, name);
    if (!DeleteFileW(STR2WPATH(path1))) {
        raise_win32_error(GetLastError(), pathsrc);
    }
    return Qnil;
}

static VALUE
ext_extattr_delete_ea(HANDLE file, VALUE path, int flags, VALUE pathsrc, VALUE name)
{
    if (file) {
        return ext_extattr_set_ea_main(file, pathsrc, name, rb_str_new(0, 0));
    }

    return ext_extattr_ctrl_fileopen(path, flags, GENERIC_WRITE, OPEN_EXISTING,
                                     pathsrc, name, rb_str_new(0, 0), ext_extattr_set_ea_main);
}

static VALUE
ext_extattr_delete_common(HANDLE file, VALUE path, int flags, VALUE pathsrc, int namespace1, VALUE name)
{
    switch (namespace1) {
    case EXTATTR_NAMESPACE_USER:
        return ext_extattr_delete_ads(file, path, pathsrc, name);
    case EXTATTR_NAMESPACE_SYSTEM:
        return ext_extattr_delete_ea(file, path, flags, pathsrc, name);
    default:
        ext_error_namespace(namespace1, pathsrc);
        return Qnil;
    }
    return Qnil;
}

static VALUE
file_extattr_delete_main(VALUE file, int fd, int namespace1, VALUE name)
{
    return ext_extattr_delete_common((HANDLE)_get_osfhandle(fd), Qnil, 0,
                                     file, namespace1, name);
}

static VALUE
file_s_extattr_delete_main(VALUE path, int namespace1, VALUE name)
{
    return ext_extattr_delete_common(0, path, 0,
                                     path, namespace1, name);
}

static VALUE
file_s_extattr_delete_link_main(VALUE path, int namespace1, VALUE name)
{
    return ext_extattr_delete_common(0, path, FILE_FLAG_OPEN_REPARSE_POINT,
                                     path, namespace1, name);
}


static void
extattr_init_implement(void)
{
    ENCutf8p = rb_enc_find("UTF-8");
    ENCutf8 = rb_enc_from_encoding(ENCutf8p);
    rb_gc_register_address(&ENCutf8);

    ENCutf16lep = rb_enc_find("UTF-16LE");
    ENCutf16le = rb_enc_from_encoding(ENCutf16lep);
    rb_gc_register_address(&ENCutf16le);

    rb_define_const(mExtAttr, "IMPLEMENT", rb_str_freeze(rb_str_new_cstr("windows")));
}
