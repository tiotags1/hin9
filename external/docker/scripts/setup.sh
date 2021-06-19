#!/bin/sh

set -e

apk update && apk upgrade

ln -s /usr/bin/php-cgi8 /usr/bin/php-cgi

USER=docker
DIR=`pwd`

mkdir $DIR/build
cd $DIR/build
cmake -GNinja ..
ninja

addgroup -g 1000 -S $USER
adduser -S --disabled-password -G $USER -u 1000 -s /bin/sh -h $DIR $USER

# needs access to build (for logs)
mkdir $DIR/workdir/logs
chown $USER:$USER $DIR/workdir/logs
chown $USER:$USER $DIR/htdocs
chmod +x -R $DIR/scripts

# remove later ?
chown -R $USER:$USER $DIR

