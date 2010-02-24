#define FUSE_USE_VERSION 26
#define _FILE_OFFSET_BITS 64

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <ruby.h>
#include <stdarg.h>

#include "fusefs_fuse.h"

#define VSTAT2STAT(cur_entry, stbuf) do {\
    stbuf->st_mode   = NUM2INT  (rb_funcall(cur_entry, rb_intern("st_mode"),  0)); \
    stbuf->st_nlink  = NUM2INT  (rb_funcall(cur_entry, rb_intern("st_nlink"), 0)); \
    stbuf->st_size   = NUM2LONG (rb_funcall(cur_entry, rb_intern("st_size"),  0)); \
    stbuf->st_uid    = NUM2INT  (rb_funcall(cur_entry, rb_intern("st_uid"),   0)); \
    stbuf->st_gid    = NUM2INT  (rb_funcall(cur_entry, rb_intern("st_gid"),   0)); \
    stbuf->st_mtime  = NUM2ULONG(rb_funcall(cur_entry, rb_intern("st_mtime"), 0)); \
    stbuf->st_atime  = NUM2ULONG(rb_funcall(cur_entry, rb_intern("st_atime"), 0)); \
    stbuf->st_ctime  = NUM2ULONG(rb_funcall(cur_entry, rb_intern("st_ctime"), 0)); \
} while (0)

#define VSTATVFS2STATVFS(cur_entry, stbuf) do {\
    memset(stbuf, 0, sizeof(struct statvfs)); \
    stbuf->f_bsize   = NUM2INT(rb_funcall(cur_entry, rb_intern("f_bsize"),    0)); \
    stbuf->f_frsize  = NUM2INT(rb_funcall(cur_entry, rb_intern("f_frsize"),   0)); \
    stbuf->f_blocks  = NUM2INT(rb_funcall(cur_entry, rb_intern("f_blocks"),   0)); \
    stbuf->f_bfree   = NUM2INT(rb_funcall(cur_entry, rb_intern("f_bfree"),    0)); \
    stbuf->f_bavail  = NUM2INT(rb_funcall(cur_entry, rb_intern("f_bavail"),   0)); \
    stbuf->f_files   = NUM2INT(rb_funcall(cur_entry, rb_intern("f_files"),    0)); \
    stbuf->f_ffree   = NUM2INT(rb_funcall(cur_entry, rb_intern("f_ffree"),    0)); \
    stbuf->f_favail  = NUM2INT(rb_funcall(cur_entry, rb_intern("f_favail"),   0)); \
    stbuf->f_fsid    = NUM2INT(rb_funcall(cur_entry, rb_intern("f_fsid"),     0)); \
    stbuf->f_flag    = NUM2INT(rb_funcall(cur_entry, rb_intern("f_flag"),     0)); \
    stbuf->f_namemax = NUM2INT(rb_funcall(cur_entry, rb_intern("f_namemax"),  0)); \
} while (0)

// Ruby Constants constants
VALUE cFuseFS      = Qnil; /* FuseFS class */
VALUE cFSException = Qnil; /* Our Exception. */
VALUE FuseRoot     = Qnil; /* The root object we call */

typedef unsigned long int (*rbfunc)();

// file created, modified timestamp
// TODO no
time_t init_time;

// debug()
static void
debug(char *msg,...)
{
    va_list ap;
    va_start(ap,msg);
    vfprintf(stdout,msg,ap);
}

// rf_protected and rf_call
//
// Used for: protection.
//
// This is called by rb_protect, and will make a call using
// the above rb_path and to_call ID to call the method safely
// on FuseRoot.
//
// We call rf_call(path,method_id), and rf_call will use rb_protect
//   to call rf_protected, which makes the call on FuseRoot and returns
//   whatever the call returns.
static VALUE
rf_protected(VALUE args)
{
  ID to_call = SYM2ID(rb_ary_shift(args));
  return rb_apply(FuseRoot,to_call,args);
}

static VALUE
rf_call(char *methname, const char *path, VALUE arg)
{
    int error;
    VALUE result;
    VALUE methargs;
    ID method;

    method = rb_intern(methname);

    if (arg == Qnil) {
        debug("    root.%s(%s)\n", methname, path);
    } else {
        debug("    root.%s(%s,...)\n", methname, path);
    }

    if (TYPE(arg) == T_ARRAY) {
        methargs = arg;
    } else if (arg != Qnil) {
        methargs = rb_ary_new();
        rb_ary_push(methargs,arg);
    } else {
        methargs = rb_ary_new();
    }

    rb_ary_unshift(methargs,rb_str_new2(path));
    rb_ary_unshift(methargs,ID2SYM(method));

    /* Set up the call and make it. */
    result = rb_protect(rf_protected, methargs, &error);

    /* Did it error? */
    if (error) {
        return Qnil;
    }

    return result;
}

