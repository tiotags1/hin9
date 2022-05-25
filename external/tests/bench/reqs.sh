
set -e

HOST=localhost:8080
HOSTS=localhost:8081

HTDOCS=htdocs/tests/

PARAMS="-q -k"

build/hin9 --quiet &

PID=$!
echo "Pid is $PID"

sleep 1

ab $PARAMS -c 100 -n 2000 http://$HOST/
ab $PARAMS -c 100 -n 2000 http://$HOST/tests/cache.php

kill $PID
