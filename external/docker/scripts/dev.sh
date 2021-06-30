#!/bin/sh

set -e

apk add nano bash sudo openssl curl coreutils xxd rsync
apk add ninja build-base cmake lua-dev linux-headers openssl-dev zlib-dev liburing-dev

DIR=`pwd`

mkdir $DIR/build
cd $DIR/build
cmake -GNinja ..
ninja

cp $DIR/build/hin9 /usr/local/bin/hinsightd

