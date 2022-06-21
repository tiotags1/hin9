
set -e

URL=http://$HOST:$PORT/proxy/tests/min.php

export RET="$(ab -k -c $BENCH_CON -n $BENCH_NUM $URL)"

sh $TOOL_DIR/hammer.sh
