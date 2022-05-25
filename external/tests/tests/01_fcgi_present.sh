
set -e

# fpm fcgi needs to be started and listening on port tcp 9000

echo "" | nc -w 1 localhost 9000
