// ruby-fuse
// A Ruby module to interact with the FUSE userland filesystem in
// a clear way.

// TODO
// * fill in the rest of the fuse api
// * turn half of these into macros
// * sample FS for testing

#include "fusefs_lib.h"

// rf_getattr               - FuseRoot.getattr should return a FuseFS::Stat object
static int
rf_getattr(const char *path, struct stat *stbuf)
{
    VALUE cur_entry;

    debug("rf_getattr(%s)\n", path);

    memset(stbuf, 0, sizeof(struct stat));
    cur_entry = rf_call("getattr", path, Qnil);

    if (RTEST(cur_entry)) {
        VSTAT2STAT(cur_entry, stbuf);
        return 0;
    } else {
        return -ENOENT;
    }
}

// rf_readlink              - resolve symlink then read bufsize into buf

// rf_mknod                 - make a special file node
static int
rf_mknod(const char *path, mode_t umode, dev_t rdev)
{
    debug("rf_mknod(%s, 0%o)\n", path, umode);

    // We ONLY permit regular files. No blocks, characters, fifos, etc.
    if (!S_ISREG(umode)) {
        return -EACCES;
    }

    // FuseFS.mknod only takes a path, since we don't handle non-files as above
    if (!RTEST(rf_call("mknod", path, Qnil)))
        return -EACCES;

    return 0;
}

// rf_mkdir                 - make a directory file
static int
rf_mkdir(const char *path, mode_t mode)
{
    VALUE ret;

    debug("rf_mkdir(%s, 0%o)\n", path, mode);

    ret = rf_call("mkdir", path, UINT2NUM(mode));

    if (RTEST(ret))
        return NUM2INT(ret);

    return -ENOENT;
}

// rf_unlink                - remove directory entry
// rm
static int
rf_unlink(const char *path)
{
    debug("rf_unlink(%s)\n", path);

    if (!RTEST(rf_call("unlink", path, Qnil)))
        return -ENOENT;

    return 0;
}

// rf_rmdir                 - remove a directory file
static int
rf_rmdir(const char *path)
{
    VALUE ret;

    debug("rf_mkdir(%s)\n", path);

    ret = rf_call("rmdir", path, Qnil);

    if (RTEST(ret))
        return NUM2INT(ret);

    return -ENOENT;
}

// rf_symlink               - make a symbolic link to a file
static int
rf_symlink(const char *path, const char *dest)
{
    VALUE args;
    VALUE ret;

    debug("rf_symlink(%s, %s)\n", path, dest);

    args = rb_ary_new();
    rb_ary_push(args, rb_str_new2(dest));

    ret = rf_call("symlink", path, args);

    if (RTEST(ret))
        return NUM2INT(ret);

    return -ENOENT;
}

// rf_rename                - change the name of a file
static int
rf_rename(const char *path, const char *dest)
{
    VALUE ret;
    VALUE args;

    debug("rf_rename(%s, %s)\n", path, dest);

    args = rb_ary_new();
    rb_ary_push(args, rb_str_new2(dest));

    ret = rf_call("rename", path, args);

    if (RTEST(ret))
        return NUM2INT(ret);

    return 0;
}

// rf_link                  - make a hard file link
static int
rf_link(const char *path, const char *dest)
{
    VALUE args;
    VALUE ret;

    debug("rf_link(%s, %s)\n", path, dest);

    args = rb_ary_new();
    rb_ary_push(args, rb_str_new2(dest));

    ret = rf_call("link", path, args);
    if (RTEST(ret))
        return NUM2INT(ret);

    return 0;
}

// rf_chmod                 - change mode of file
static int
rf_chmod(const char *path, mode_t mode)
{
    VALUE ret;

    debug("rf_chmod(%s, %o)\n", path, mode);

    ret = rf_call("chmod", path, INT2NUM(mode));

    if (RTEST(ret))
        return NUM2INT(ret);

    return -ENOENT;
}

// rf_chown                 - change owner and group of a file

// rf_truncate              - truncate or extend a file to a specified length
// delete backwards until offset
static int
rf_truncate(const char *path, off_t offset)
{
    VALUE ret;

    debug("rf_truncate(%s, %d)\n", path, offset);

    ret = rf_call("truncate", path, ULONG2NUM(offset));

    if (RTEST(ret))
        return NUM2INT(ret);

    return -ENOENT;
}

// rf_utime                 - set file times
static int
rf_utime(const char *path, struct utimbuf *ubuf)
{
    VALUE ret;
    VALUE args;

    args = rb_ary_new();
    rb_ary_push(args, LONG2NUM(ubuf->actime));
    rb_ary_push(args, LONG2NUM(ubuf->modtime));

    ret = rf_call("utime", path, args);

    if (RTEST(ret))
        return ret;

    // default to non-error because I don't see myself using this much
    return 0;
}

// rf_open                  - open or create a file for reading or writing
// opening the file will probably be a noop for most backends
static int
rf_open(const char *path, struct fuse_file_info *fi)
{
    char open_opts[4], *optr;

    debug("rf_open(%s)\n", path);

    optr = open_opts;
    switch (fi->flags & 3) {
        case 0:
            *(optr++) = 'r';
            break;
        case 1:
            *(optr++) = 'w';
            break;
        case 2:
            *(optr++) = 'w';
            *(optr++) = 'r';
            break;
        default:
            debug("Opening a file with something other than rd, wr, or rdwr?");
    }

    if (fi->flags & O_APPEND)
        *(optr++) = 'a';
    *(optr) = '\0';

    if (!RTEST(rf_call("open", path, rb_str_new2(open_opts)))) {
        return -ENOENT;
    }

    return 0;
}

// rf_read                  - read input
static int
rf_read(
    const char *path,
    char *buf,
    size_t size,
    off_t offset,
    struct fuse_file_info *fi)
{
    VALUE ret;
    VALUE args;

    debug("rf_read(%s)\n", path);

    args = rb_ary_new();
    rb_ary_push(args, LONG2NUM(offset));
    rb_ary_push(args, LONG2NUM(size));

    ret = rf_call("read", path, args);

    if (!RTEST(ret))
        return 0;

    if (TYPE(ret) != T_STRING)
        return 0;

    memcpy(buf, RSTRING(ret)->ptr, RSTRING(ret)->len);
    return RSTRING(ret)->len;
}

// rf_write                 - write output
static int
rf_write(
    const char *path,
    const char *buf,
    size_t size,
    off_t offset,
    struct fuse_file_info *fi)
{
    VALUE ret;
    VALUE args;

    debug("rf_write(%s)\n", path);

    args = rb_ary_new();
    rb_ary_push(args, ULONG2NUM(offset));
    rb_ary_push(args, ULONG2NUM(size));
    rb_ary_push(args, rb_str_new(buf, size));

    ret = rf_call("write", path, args);

    if (RTEST(ret))
        return NUM2INT(ret);

    return 0UL;
}

// rf_statfs                - get filesystem statistics
static int
rf_statfs(const char *path, struct statvfs *stbuf)
{
    return 0;
    // VALUE cur_entry;

    // debug("rf_statfs(%s)\n", path);

    // cur_entry = rf_call("statfs", path, Qnil);

    // if (RTEST(cur_entry)) {
    //     VSTATVFS2STATVFS(cur_entry, stbuf);
    //     return 0;
    // } else {
    //     return -ENOENT;
    // }
}

// rf_flush                 - close file (after system calls close())

// rf_release               - close file (the final flush)
static int
rf_release(const char *path, struct fuse_file_info *fi)
{
    debug("rf_release(%s)\n", path);

    if (!RTEST(rf_call("release", path, Qnil)))
        return -ENOENT;

    return 0;
}

// rf_fsync                 - synchronise a file's in-core state with that on disk

// rf_setxattr              - set an extended attribute value

// rf_getxattr              - get an extended attribute value

// rf_listxattr             - list extended attribute names

// rf_removexattr           - remove an extended attribute value

// rf_opendir               - check if opendir is permitted for this directory

// rf_readdir               - an array of FuseFS::Stat objects sent to fill_dir
static int
rf_readdir(
    const char *path,
    void *buf,
    fuse_fill_dir_t filler,
    off_t offset,
    struct fuse_file_info *fi)
{
    VALUE cur_entry;
    VALUE retval;
    struct stat *stbuf;

    debug("rf_readdir(%s)\n", path);

    stbuf = malloc(sizeof(struct stat));

    // TODO maybe fi is interesting
    // This is what fuse does to turn off 'unused' warnings.
    (void) offset;
    (void) fi;

    // filler takes (void *dh_, const char *name, const struct stat *statp, off_t off)
    // you can pass in NULL statp for defaults
    // you can pass in 0 off for magic

    // These two are always in a directory
    filler(buf, ".",  NULL, 0);
    filler(buf, "..", NULL, 0);

    retval = rf_call("readdir", path, Qnil);
    if (!RTEST(retval) || (TYPE(retval) != T_ARRAY)) {
        return 0;
    }

    while ((cur_entry = rb_ary_shift(retval)) != Qnil) {
        VSTAT2STAT(cur_entry, stbuf);
        filler(buf, STR2CSTR(rb_funcall(cur_entry, rb_intern("name"), 0)), stbuf, 0);
    }

    return 0;
}

// rf_releasedir            - release dir (file returned from opendir is passed in)

// rf_fsyncdir              - sync everything in the dir
static int
rf_fsyncdir(const char * path, int p, struct fuse_file_info *fi)
{
    VALUE ret;
    VALUE args;

    // non-0 p means user data only, not metadata
    args = rb_ary_new();
    rb_ary_push(args, INT2NUM(p));

    ret = rf_call("fsyncdir", path, args);

    if (RTEST(ret))
        return NUM2INT(ret);

    // default to non-error because I don't see myself using this much
    return 0;
}

// rf_access                - check access permissions of a file or pathname
static int
rf_access(const char *path, int mask)
{
    debug("rf_access(%s, %d)\n", path, mask);
    return 0;
}

// rf_create                - create and open a file

// rf_ftruncate             - truncate() but called from system ftruncate()

// rf_fgetattr              - getattr() but called if rf_create() is implemented

// rf_lock                  - perform POSIX file locking operation

// rf_utimens               - utime with nanosecond resolution

// rf_bmap                  - map block index within file to block index within device

// rf_flag_nullpath_ok      - accept NULL path


// rf_oper
static struct fuse_operations rf_oper = {
    .getattr    = rf_getattr,
    .readdir    = rf_readdir,
    .mknod      = rf_mknod,
    .mkdir      = rf_mkdir,
    .unlink     = rf_unlink,
    .rmdir      = rf_rmdir,
    .rename     = rf_rename,
    .truncate   = rf_truncate,
    .utime      = rf_utime,
    .open       = rf_open,
    .read       = rf_read,
    .write      = rf_write,
    .statfs     = rf_statfs,
    .release    = rf_release,
    .fsyncdir   = rf_fsyncdir,
};

static struct fuse_operations rf_unused_oper = {
    .symlink    = rf_symlink,
    .access     = rf_access,
    .link       = rf_link,
    .chmod      = rf_chmod,
};

// rf_set_root
VALUE
rf_set_root(VALUE self, VALUE rootval)
{
    if (self != cFuseFS) {
        rb_raise(cFSException, "Error: 'set_root' called outside of FuseFS?!");
        return Qnil;
    }

    rb_iv_set(cFuseFS, "@root", rootval);
    FuseRoot = rootval;

    return Qtrue;
}

// rf_mount_to
VALUE
rf_mount_to(int argc, VALUE *argv, VALUE self)
{
    struct fuse_args *opts;
    VALUE mountpoint;
    int i;
    char *cur;

    if (self != cFuseFS) {
        rb_raise(cFSException, "Error: 'mount_to' called outside of FuseFS?!");
        return Qnil;
    }

    if (argc == 0) {
        rb_raise(rb_eArgError, "mount_to requires at least 1 argument!");
        return Qnil;
    }

    mountpoint = argv[0];

    Check_Type(mountpoint, T_STRING);
    opts = ALLOC(struct fuse_args);
    opts->argc = argc;
    opts->argv = ALLOC_N(char *, opts->argc);
    opts->allocated = 1;

    // TODO why
    opts->argv[0] = strdup("-odirect_io");

    for (i = 1; i < argc; i++) {
        cur = StringValuePtr(argv[i]);
        opts->argv[i] = ALLOC_N(char, RSTRING(argv[i])->len + 2);
        sprintf(opts->argv[i], "-o%s", cur);
    }

    // TODO debug(rf_unused_oper);
    (void) rf_unused_oper;

    rb_iv_set(cFuseFS, "@mountpoint", mountpoint);
    fusefs_setup(STR2CSTR(mountpoint), &rf_oper, opts);

    return Qtrue;
}

// rf_fd
VALUE
rf_fd(VALUE self)
{
    int fd = fusefs_fd();
    if (fd < 0)
        return Qnil;
    return INT2NUM(fd);
}

// rf_process
VALUE
rf_process(VALUE self)
{
    fusefs_process();
    return Qnil;
}

// Init_fusefs_lib()
void
Init_fusefs_lib() {
    init_time = time(NULL);

    /* module FuseFS */
    cFuseFS = rb_define_module("FuseFS");

    /* Our exception */
    cFSException = rb_define_class_under(cFuseFS, "FuseFSException",
        rb_eStandardError);

    /* def Fuse.run */
    rb_define_singleton_method(cFuseFS, "fuse_fd",      rf_fd,           0);
    rb_define_singleton_method(cFuseFS, "process",      rf_process,      0);
    rb_define_singleton_method(cFuseFS, "mount_to",     rf_mount_to,    -1);
    rb_define_singleton_method(cFuseFS, "mount_under",  rf_mount_to,    -1);
    rb_define_singleton_method(cFuseFS, "mountpoint",   rf_mount_to,    -1);
    rb_define_singleton_method(cFuseFS, "set_root",     rf_set_root,     1);
    rb_define_singleton_method(cFuseFS, "root=",        rf_set_root,     1);
}

