#!/bin/bash

set -e -x

WORKDIR=/tmp/work

rm -rf $WORKDIR

for d in xenial artful bionic; do
  git-build-recipe --allow-fallback-to-native \
    --package littlefs-fuse \
    --distribution $d $PWD/debian/littlefs-fuse-$d.recipe /tmp/work
  rm -rf $WORKDIR/littlefs-fuse
done
