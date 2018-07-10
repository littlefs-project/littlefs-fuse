## The little filesystem in user-space

A FUSE wrapper that puts the littlefs in user-space.

**FUSE** - https://github.com/libfuse/libfuse  
**littlefs** - https://github.com/geky/littlefs  

This project allows you to mount littlefs directly in a host PC.
This allows you to easily debug an embedded system using littlefs on
removable storage, or even debug littlefs itself, since the block device
can be viewed in a hex-editor simultaneously.

littlefs-fuse uses FUSE to interact with the host OS kernel, which means
it can be compiled into a simple user program without kernel modifications.
This comes with a performance penalty, but works well for the littlefs,
since littlefs is intended for embedded systems.

Currently littlefs-fuse has been tested on the following OSs:
- [Linux](#usage-on-linux)
- [FreeBSD](#usage-on-freebsd)

## Usage on Linux

littlefs-fuse requires FUSE version 2.6 or higher, you can find your FUSE
version with:
``` bash
fusermount -V
```

In order to build against FUSE, you will need the package `libfuse-dev`:
``` bash
sudo apt-get install libfuse-dev
```

Once you have cloned littlefs-fuse, you can compile the program with make:
``` bash
make
```

This should have built the `lfs` program in the top-level directory.

From here we will need a block device. If you don't have removable storage
handy, you can use a file-backed block device with Linux's loop devices:
``` bash
sudo chmod a+rw /dev/loop0                  # make loop device user accessible
dd if=/dev/zero of=image bs=512 count=2048  # create a 1MB image
losetup /dev/loop0 image                    # attach the loop device
```

littlefs-fuse has two modes of operation, formatting and mounting.

To format a block device, pass the `--format` flag. Note! This will erase any
data on the block device!
``` bash
./lfs --format /dev/loop0
```

To mount, run littlefs-fuse with a block device and a mountpoint:
``` bash
mkdir mount
./lfs /dev/loop0 mount
```

Once mounted, the littlefs filesystem will be accessible through the
mountpoint. You can now use the littlefs like you would any other filesystem:

``` bash
cd mount
echo "hello" > hi.txt
ls
cat hi.txt
```

After using littlefs, you can unmount and detach the loop device:
``` bash
cd ..
umount mount
sudo losetup -d /dev/loop0
```

## Usage on FreeBSD

littlefs-fuse requires FUSE version 2.6 or higher, you can find your FUSE
version with:
``` bash
pkg info fusefs-libs | grep Version
```

Once you have cloned littlefs-fuse, you can compile the program with make:
``` bash
gmake
```

This should have built the `lfs` program in the top-level directory.

From here we will need a block device. If you don't have removable storage
handy, you can use a file-backed block device with FreeBSD's loop devices:
``` bash
dd if=/dev/zero of=imageBSD bs=1m count=1   # create a 1 MB image
sudo mdconfig -at vnode -f image            # attach the loop device
sudo chmod 666 /dev/mdX                     # make loop device user accessible,
                                            # where mdX is device created with mdconfig command
```

littlefs-fuse has two modes of operation, formatting and mounting.

To format a block device, pass the `--format` flag. Note! This will erase any
data on the block device!
``` bash
./lfs --format /dev/md0
```

To mount, run littlefs-fuse with a block device and a mountpoint:
``` bash
mkdir mount
./lfs /dev/md0 mount
```

Once mounted, the littlefs filesystem will be accessible through the
mountpoint. You can now use the littlefs like you would any other filesystem:
``` bash
cd mount
echo "hello" > hi.txt
ls
cat hi.txt
```

After using littlefs, you can unmount and detach the loop device:
``` bash
cd ..
umount mount
sudo mdconfig -du 0
```

## Limitations

As an embedded filesystem, littlefs is designed to be simple. By default,
this comes with a number of limitations compared to more PC oriented
filesystems:

- No timestamps, this will cause some programs, such as make to fail
- No user permissions, this is why all of the files show up bright green
  in ls, all files are accessible by anyone
- No symbolic links or special device files, currently only regular and
  directory file-types are implemented

## Tips

If the littlefs was formatted with different geometry than the physical block
device, you can override what littlefs-fuse detects. `lfs -h` lists all
available options:
``` bash
./lfs --block_size=512 --format /dev/loop0
./lfs --block_size=512 /dev/loop0 mount
```

You can run littlefs-fuse in debug mode to get a log of the kernel interactions
with littlefs-fuse. Any printfs in the littlefs driver will end up here:
``` bash
./lfs -d /dev/loop0 mount
```

You can even run littlefs-fuse in gdb to debug the filesystem under user
operations. Note! When gdb is halted this will freeze any programs interacting
with the filesystem!
``` bash
make DEBUG=1 clean all                # build with debug info
gdb --args ./lfs -d /dev/loop0 mount  # run with gdb
```

Using xxd or other hex-editory is very useful for inspecting the block
device while debugging. You can even run xxd from inside gdb using gdb's
`!` syntax:
``` bash
dd if=/dev/loop0 bs=512 count=1 skip=0 | xxd -g1
```
