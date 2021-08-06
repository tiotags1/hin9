
set -e

HOST=localhost:8080
HOSTS=localhost:8081
HTDOCS=htdocs
DIR=tests/
TEST=big.bin
TEST_FILE=$HTDOCS/$DIR$TEST
PARAMS="--no-check-certificate"
PID_FILE=/tmp/test.pid

dd if=/dev/zero of=$TEST_FILE bs=64M count=8 iflag=fullblock &> /dev/null

build/hin9 --quiet --daemonize --pidfile $PID_FILE &> /dev/null

sleep 1

PID=`cat $PID_FILE`
echo "Pid is $PID"

# test
wget $PARAMS http://$HOST/$DIR$TEST -O /dev/null
wget $PARAMS https://$HOSTS/$DIR$TEST -O /dev/null

# cleanup
rm $TEST_FILE
kill $PID
