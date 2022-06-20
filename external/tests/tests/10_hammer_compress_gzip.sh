
set -e

URL=http://$HOST:$PORT/

export RET="$(ab -k -H "Accept-Encoding: gzip" -c $BENCH_CON -n $BENCH_NUM $URL)"

sh $TOOL_DIR/hammer.sh

