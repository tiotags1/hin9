
set -e

ln -s /usr/bin/php-cgi8 /usr/bin/php-cgi

mkdir build
cd build
cmake -GNinja ..
ninja

addgroup -g 1000 -S docker
adduser -S --disabled-password -G docker -u 1000 -s /bin/sh -h /app docker

