
set -e

URL=https://$HOST:$PORTS/

export RET="$(ab -k -c $BENCH_CON -n $BENCH_NUM $URL)"

sh $TOOL_DIR/hammer.sh
