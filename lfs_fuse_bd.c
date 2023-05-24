/*
 * Linux user-space block device wrapper
 *
 * Copyright (c) 2017, Arm Limited. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "lfs_fuse_bd.h"

#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>
#include <assert.h>
#if !defined(__FreeBSD__)
#include <sys/ioctl.h>
#include <linux/fs.h>
#elif defined(__FreeBSD__)
#define BLKSSZGET DIOCGSECTORSIZE
#define BLKGETSIZE DIOCGMEDIASIZE
#include <sys/disk.h>
#endif


// Block device wrapper for user-space block devices
int lfs_fuse_bd_create(struct lfs_config *cfg, const char *path) {
    int fd = open(path, O_RDWR);
    if (fd < 0) {
        return -errno;
    }
    cfg->context = (void*)(intptr_t)fd;

    // get sector size
    if (!cfg->block_size) {
        long ssize;
        int err = ioctl(fd, BLKSSZGET, &ssize);
        if (err) {
            return -errno;
        }
        cfg->block_size = ssize;
    }

    // get size in sectors
    if (!cfg->block_count) {
        long size;
        int err = ioctl(fd, BLKGETSIZE, &size);
        if (err) {
            return -errno;
        }
        cfg->block_count = size;
    }

    // setup function pointers
    cfg->read  = lfs_fuse_bd_read;
    cfg->prog  = lfs_fuse_bd_prog;
    cfg->erase = lfs_fuse_bd_erase;
    cfg->sync  = lfs_fuse_bd_sync;

    return 0;
}

void lfs_fuse_bd_destroy(const struct lfs_config *cfg) {
    int fd = (intptr_t)cfg->context;
    close(fd);
}

int lfs_fuse_bd_read(const struct lfs_config *cfg, lfs_block_t block,
        lfs_off_t off, void *buffer, lfs_size_t size) {
    int fd = (intptr_t)cfg->context;

    // check if read is valid
    assert(block < cfg->block_count);

    // go to block
    off_t offrq = (off_t)block*cfg->block_size + (off_t)off;
    off_t err = lseek(fd,offrq, SEEK_SET);
    if (err < 0) {
        return -errno;
    } else if( err != offrq ) {
        return LFS_ERR_IO;
    }
    char* rdb = buffer;
    do {
        // read block
        ssize_t res = read(fd, rdb, (size_t)size);
        if (res <= 0) {
            return (res==0)?(LFS_ERR_IO):(-errno);
        } else {
            rdb += res;
            size -= res;
        }
    } while( size > 0 );
    return 0;
}

int lfs_fuse_bd_prog(const struct lfs_config *cfg, lfs_block_t block,
        lfs_off_t off, const void *buffer, lfs_size_t size) {
    int fd = (intptr_t)cfg->context;

    // check if write is valid
    assert(block < cfg->block_count);

    // go to block
    off_t offrq = (off_t)block*cfg->block_size + (off_t)off;
    off_t err = lseek(fd,offrq, SEEK_SET);
    if (err < 0) {
        return -errno;
    } else if(err != offrq )
    {
        return LFS_ERR_IO;
    }
    const char* wrb = buffer;
    do {
        // write block
        ssize_t res = write(fd, wrb, (size_t)size);
        if (res <= 0) {
            return (res==0)?(LFS_ERR_IO):(-errno);
        } else {
            wrb += res;
            size -= res;
        }
    } while(0);

    return 0;
}

int lfs_fuse_bd_erase(const struct lfs_config *cfg, lfs_block_t block) {
    // do nothing
    return 0;
}

int lfs_fuse_bd_sync(const struct lfs_config *cfg) {
    int fd = (intptr_t)cfg->context;

    int err = fsync(fd);
    if (err) {
        return -errno;
    }
    return 0;
}

