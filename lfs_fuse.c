/*
 * FUSE wrapper for the littlefs
 *
 * Copyright (c) 2017, Arm Limited. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#define FUSE_USE_VERSION 26

#ifdef linux
// needed for a few things fuse depends on
#define _XOPEN_SOURCE 700
#endif

#include <fuse/fuse.h>
#include "lfs.h"
#include "lfs_util.h"
#include "lfs_fuse_bd.h"

#include <stdio.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>


// config and other state
static struct lfs_config config = {0};
static const char *device = NULL;
static bool format = false;
static bool migrate = false;
static lfs_t lfs;


// actual fuse functions
void lfs_fuse_defaults(struct lfs_config *config) {
    // default to 512 erase cycles, arbitrary value
    if (!config->block_cycles) {
        config->block_cycles = 512;
    }

    // defaults, ram is less of a concern here than what
    // littlefs is used to, so these may end up a bit funny
    if (!config->prog_size) {
        config->prog_size = config->block_size;
    }

    if (!config->read_size) {
        config->read_size = config->block_size;
    }

    if (!config->cache_size) {
        config->cache_size = config->block_size;
    }

    // arbitrary, though we have a lot of RAM here
    if (!config->lookahead_size) {
        config->lookahead_size = 8192;
    }
}

void *lfs_fuse_init(struct fuse_conn_info *conn) {
    // set that we want to take care of O_TRUNC
    conn->want |= FUSE_CAP_ATOMIC_O_TRUNC;

    // we also support writes of any size
    conn->want |= FUSE_CAP_BIG_WRITES;

    return 0;
}

int lfs_fuse_format(void) {
    int err = lfs_fuse_bd_create(&config, device);
    if (err) {
        return err;
    }

    lfs_fuse_defaults(&config);

    err = lfs_format(&lfs, &config);

    lfs_fuse_bd_destroy(&config);
    return err;
}

int lfs_fuse_migrate(void) {
    int err = lfs_fuse_bd_create(&config, device);
    if (err) {
        return err;
    }

    lfs_fuse_defaults(&config);

    err = lfs_migrate(&lfs, &config);

    lfs_fuse_bd_destroy(&config);
    return err;
}

int lfs_fuse_mount(void) {
    int err = lfs_fuse_bd_create(&config, device);
    if (err) {
        return err;
    }

    lfs_fuse_defaults(&config);

    return lfs_mount(&lfs, &config);
}

void lfs_fuse_destroy(void *eh) {
    lfs_unmount(&lfs);
    lfs_fuse_bd_destroy(&config);
}

int lfs_fuse_statfs(const char *path, struct statvfs *s) {
    memset(s, 0, sizeof(struct statvfs));

    lfs_ssize_t in_use = lfs_fs_size(&lfs);
    if (in_use < 0) {
        return in_use;
    }

    s->f_bsize = config.block_size;
    s->f_frsize = config.block_size;
    s->f_blocks = config.block_count;
    s->f_bfree = config.block_count - in_use;
    s->f_bavail = config.block_count - in_use;
    s->f_namemax = config.name_max;

    return 0;
}

static void lfs_fuse_tostat(struct stat *s, struct lfs_info *info) {
    memset(s, 0, sizeof(struct stat));

    s->st_size = info->size;
    s->st_mode = S_IRWXU | S_IRWXG | S_IRWXO;

    switch (info->type) {
        case LFS_TYPE_DIR: s->st_mode |= S_IFDIR; break;
        case LFS_TYPE_REG: s->st_mode |= S_IFREG; break;
    }
}

int lfs_fuse_getattr(const char *path, struct stat *s) {
    struct lfs_info info;
    int err = lfs_stat(&lfs, path, &info);
    if (err) {
        return err;
    }

    lfs_fuse_tostat(s, &info);
    return 0;
}

int lfs_fuse_access(const char *path, int mask) {
    struct lfs_info info;
    return lfs_stat(&lfs, path, &info);
}

int lfs_fuse_mkdir(const char *path, mode_t mode) {
    return lfs_mkdir(&lfs, path);
}

int lfs_fuse_opendir(const char *path, struct fuse_file_info *fi) {
    lfs_dir_t *dir = malloc(sizeof(lfs_dir_t));
    memset(dir, 0, sizeof(lfs_dir_t));

    int err = lfs_dir_open(&lfs, dir, path);
    if (err) {
        free(dir);
        return err;
    }

    fi->fh = (uint64_t)dir;
    return 0;
}

int lfs_fuse_releasedir(const char *path, struct fuse_file_info *fi) {
    lfs_dir_t *dir = (lfs_dir_t*)fi->fh;

    int err = lfs_dir_close(&lfs, dir);
    free(dir);
    return err;
}

int lfs_fuse_readdir(const char *path, void *buf,
        fuse_fill_dir_t filler, off_t offset,
        struct fuse_file_info *fi) {
    
    lfs_dir_t *dir = (lfs_dir_t*)fi->fh;
    struct stat s;
    struct lfs_info info;

    while (true) {
        int err = lfs_dir_read(&lfs, dir, &info);
        if (err != 1) {
            return err;
        }

        lfs_fuse_tostat(&s, &info);
        filler(buf, info.name, &s, 0);
    }
}

int lfs_fuse_rename(const char *from, const char *to) {
    return lfs_rename(&lfs, from, to);
}

int lfs_fuse_unlink(const char *path) {
    return lfs_remove(&lfs, path);
}

int lfs_fuse_open(const char *path, struct fuse_file_info *fi) {
    lfs_file_t *file = malloc(sizeof(lfs_file_t));
    memset(file, 0, sizeof(lfs_file_t));

    int flags = 0;
    if ((fi->flags & 3) == O_RDONLY) flags |= LFS_O_RDONLY;
    if ((fi->flags & 3) == O_WRONLY) flags |= LFS_O_WRONLY;
    if ((fi->flags & 3) == O_RDWR)   flags |= LFS_O_RDWR;
    if (fi->flags & O_CREAT)         flags |= LFS_O_CREAT;
    if (fi->flags & O_EXCL)          flags |= LFS_O_EXCL;
    if (fi->flags & O_TRUNC)         flags |= LFS_O_TRUNC;
    if (fi->flags & O_APPEND)        flags |= LFS_O_APPEND;

    int err = lfs_file_open(&lfs, file, path, flags);
    if (err) {
        free(file);
        return err;
    }

    fi->fh = (uint64_t)file;
    return 0;
}

int lfs_fuse_release(const char *path, struct fuse_file_info *fi) {
    lfs_file_t *file = (lfs_file_t*)fi->fh;

    int err = lfs_file_close(&lfs, file);
    free(file);
    return err;
}

int lfs_fuse_fgetattr(const char *path, struct stat *s,
        struct fuse_file_info *fi) {
    lfs_file_t *file = (lfs_file_t*)fi->fh;

    lfs_fuse_tostat(s, &(struct lfs_info){
        .size = lfs_file_size(&lfs, file),
        .type = LFS_TYPE_REG,
    });

    return 0;
}

int lfs_fuse_read(const char *path, char *buf, size_t size,
        off_t off, struct fuse_file_info *fi) {
    lfs_file_t *file = (lfs_file_t*)fi->fh;

    if (lfs_file_tell(&lfs, file) != off) {
        lfs_soff_t soff = lfs_file_seek(&lfs, file, off, LFS_SEEK_SET);
        if (soff < 0) {
            return soff;
        }
    }

    return lfs_file_read(&lfs, file, buf, size);
}

int lfs_fuse_write(const char *path, const char *buf, size_t size,
        off_t off, struct fuse_file_info *fi) {
    lfs_file_t *file = (lfs_file_t*)fi->fh;

    if (lfs_file_tell(&lfs, file) != off) {
        lfs_soff_t soff = lfs_file_seek(&lfs, file, off, LFS_SEEK_SET);
        if (soff < 0) {
            return soff;
        }
    }

    return lfs_file_write(&lfs, file, buf, size);
}

int lfs_fuse_fsync(const char *path, int isdatasync,
        struct fuse_file_info *fi) {
    lfs_file_t *file = (lfs_file_t*)fi->fh;
    return lfs_file_sync(&lfs, file);
}

int lfs_fuse_flush(const char *path, struct fuse_file_info *fi) {
    lfs_file_t *file = (lfs_file_t*)fi->fh;
    return lfs_file_sync(&lfs, file);
}

int lfs_fuse_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    int err = lfs_fuse_open(path, fi);
    if (err) {
        return err;
    }

    return lfs_fuse_fsync(path, 0, fi);
}

int lfs_fuse_ftruncate(const char *path, off_t size,
        struct fuse_file_info *fi) {
    lfs_file_t *file = (lfs_file_t*)fi->fh;
    return lfs_file_truncate(&lfs, file, size);
}

int lfs_fuse_truncate(const char *path, off_t size) {
    lfs_file_t file;
    int err = lfs_file_open(&lfs, &file, path, LFS_O_WRONLY);
    if (err) {
        return err;
    }

    err = lfs_file_truncate(&lfs, &file, size);
    if (err) {
        return err;
    }

    return lfs_file_close(&lfs, &file);
}

// unsupported functions
int lfs_fuse_link(const char *from, const char *to) {
    // not supported, fail
    return -EPERM;
}

int lfs_fuse_mknod(const char *path, mode_t mode, dev_t dev) {
    // not supported, fail
    return -EPERM;
}

int lfs_fuse_chmod(const char *path, mode_t mode) {
    // not supported, always succeed
    return 0;
}

int lfs_fuse_chown(const char *path, uid_t uid, gid_t gid) {
    // not supported, fail
    return -EPERM;
}

int lfs_fuse_utimens(const char *path, const struct timespec ts[2]) {
    // not supported, always succeed
    return 0;
}

static struct fuse_operations lfs_fuse_ops = {
    .init       = lfs_fuse_init,
    .destroy    = lfs_fuse_destroy,
    .statfs     = lfs_fuse_statfs,

    .getattr    = lfs_fuse_getattr,
    .access     = lfs_fuse_access,

    .mkdir      = lfs_fuse_mkdir,
    .rmdir      = lfs_fuse_unlink,
    .opendir    = lfs_fuse_opendir,
    .releasedir = lfs_fuse_releasedir,
    .readdir    = lfs_fuse_readdir,

    .rename     = lfs_fuse_rename,
    .unlink     = lfs_fuse_unlink,

    .open       = lfs_fuse_open,
    .create     = lfs_fuse_create,
    .truncate   = lfs_fuse_truncate,
    .release    = lfs_fuse_release,
    .fgetattr   = lfs_fuse_fgetattr,
    .read       = lfs_fuse_read,
    .write      = lfs_fuse_write,
    .fsync      = lfs_fuse_fsync,
    .flush      = lfs_fuse_flush,

    .link       = lfs_fuse_link,
    .symlink    = lfs_fuse_link,
    .mknod      = lfs_fuse_mknod,
    .chmod      = lfs_fuse_chmod,
    .chown      = lfs_fuse_chown,
    .utimens    = lfs_fuse_utimens,
};


// binding into fuse and general ui
enum lfs_fuse_keys {
    KEY_HELP,
    KEY_VERSION,
    KEY_FORMAT,
    KEY_MIGRATE,
};

#define OPT(t, p) { t, offsetof(struct lfs_config, p), 0}
static struct fuse_opt lfs_fuse_opts[] = {
    FUSE_OPT_KEY("--format",    KEY_FORMAT),
    FUSE_OPT_KEY("--migrate",   KEY_MIGRATE),
    OPT("-b=%"                  SCNu32, block_size),
    OPT("--block_size=%"        SCNu32, block_size),
    OPT("--block_count=%"       SCNu32, block_count),
    OPT("--block_cycles=%"      SCNu32, block_cycles),
    OPT("--read_size=%"         SCNu32, read_size),
    OPT("--prog_size=%"         SCNu32, prog_size),
    OPT("--cache_size=%"        SCNu32, cache_size),
    OPT("--lookahead_size=%"    SCNu32, lookahead_size),
    OPT("--name_max=%"          SCNu32, name_max),
    OPT("--file_max=%"          SCNu32, file_max),
    OPT("--attr_max=%"          SCNu32, attr_max),
    FUSE_OPT_KEY("-V",          KEY_VERSION),
    FUSE_OPT_KEY("--version",   KEY_VERSION),
    FUSE_OPT_KEY("-h",          KEY_HELP),
    FUSE_OPT_KEY("--help",      KEY_HELP),
    FUSE_OPT_END
};

static const char help_text[] =
"usage: %s [options] device mountpoint\n"
"\n"
"general options:\n"
"    -o opt,[opt...]        FUSE options\n"
"    -h   --help            print help\n"
"    -V   --version         print version\n"
"\n"
"littlefs options:\n"
"    --format               format instead of mounting\n"
"    --migrate              migrate previous version  instead of mounting\n"
"    -b   --block_size      logical block size, overrides the block device\n"
"    --block_count          block count, overrides the block device\n"
"    --block_cycles         number of erase cycles before eviction (512)\n"
"    --read_size            readable unit (block_size)\n"
"    --prog_size            programmable unit (block_size)\n"
"    --cache_size           size of caches (block_size)\n"
"    --lookahead_size       size of lookahead buffer (8192)\n"
"    --name_max             max size of file names (255)\n"
"    --file_max             max size of file contents (2147483647)\n"
"    --attr_max             max size of custom attributes (1022)\n"
"\n";

int lfs_fuse_opt_proc(void *data, const char *arg,
        int key, struct fuse_args *args) {

    // option parsing
    switch (key) {
        case FUSE_OPT_KEY_NONOPT:
            if (!device) {
                device = strdup(arg);
                return 0;
            }
            break;

        case KEY_FORMAT:
            format = true;
            return 0;

        case KEY_MIGRATE:
            migrate = true;
            return 0;
            
        case KEY_HELP:
            fprintf(stderr, help_text, args->argv[0]);
            fuse_opt_add_arg(args, "-ho");
            fuse_main(args->argc, args->argv, &lfs_fuse_ops, NULL);
            exit(1);
            
        case KEY_VERSION:
            fprintf(stderr, "littlefs version: v%d.%d\n",
                 LFS_VERSION_MAJOR, LFS_VERSION_MINOR);
            fprintf(stderr, "littlefs disk version: v%d.%d\n",
                 LFS_DISK_VERSION_MAJOR, LFS_DISK_VERSION_MINOR);
            fuse_opt_add_arg(args, "--version");
            fuse_main(args->argc, args->argv, &lfs_fuse_ops, NULL);
            exit(0);
    }

    return 1;
}

int main(int argc, char *argv[]) {
    // parse custom options
    struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
    fuse_opt_parse(&args, &config, lfs_fuse_opts, lfs_fuse_opt_proc);
    if (!device) {
        fprintf(stderr, "missing device parameter\n");
        exit(1);
    }

    if (format) {
        // format time, no mount
        int err = lfs_fuse_format();
        if (err) {
            LFS_ERROR("%s", strerror(-err));
            exit(-err);
        }
        exit(0);
    }

    if (migrate) {
        // migrate time, no mount
        int err = lfs_fuse_migrate();
        if (err) {
            LFS_ERROR("%s", strerror(-err));
            exit(-err);
        }
        exit(0);
    }

    // go ahead and mount so errors are reported before backgrounding
    int err = lfs_fuse_mount();
    if (err) {
        LFS_ERROR("%s", strerror(-err));
        exit(-err);
    }

    // always single-threaded
    fuse_opt_add_arg(&args, "-s");

    // enter fuse
    err = fuse_main(args.argc, args.argv, &lfs_fuse_ops, NULL);
    if (err) {
        lfs_fuse_destroy(NULL);
    }

    return err;
}
