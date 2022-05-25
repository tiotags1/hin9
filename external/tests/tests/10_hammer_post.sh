
set -e

URL=http://$HOST:$PORT/tests/post.php

RET="$(ab -k -c 1000 -n 10000 $URL)"

echo "$RET"

total=`echo "$RET" 2>&1 | grep "Requests per second"`

echo "php POST-ing $total" >> $run_dir/bench.txt
