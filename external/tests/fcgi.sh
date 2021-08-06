
set -e

HOST=localhost:8080
HOSTS=localhost:8081
HTDOCS=htdocs
DIR=tests/
TEST=post.php
PARAMS="-k"
PID_FILE=/tmp/test.pid

build/hin9 --quiet --daemonize --pidfile $PID_FILE &> /dev/null

sleep 1

PID=`cat $PID_FILE`
echo "Pid is $PID"

# test
VALUE=hello
echo "VALUE to search is $VALUE"

curl $PARAMS --form VALUE="$VALUE" http://$HOST/$DIR$TEST | grep VALUE

# cleanup
kill $PID
