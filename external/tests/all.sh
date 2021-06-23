
set -e

HOST=localhost:8080
HOSTS=localhost:8081

HTDOCS=htdocs/tests/

PROXY=

PARAMS="-q -k"

build/hin9 --quiet &

PID=$!
echo "Pid is $PID"

sleep 1

ab $PARAMS -c 100 -n 2000 http://$HOST/
ab $PARAMS -c 100 -n 2000 http://$HOST/tests/cache.php
ab $PARAMS -c 100 -n 200 https://$HOSTS/

kill $PID
